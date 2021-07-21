﻿#include "UnLuaPrivate.h"
#include "UnLuaEditorCore.h"
#include "UnLuaEditorToolbar.h"
#include "UnLuaEditorCommands.h"
#include "UnLuaInterface.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "BlueprintEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

#define LOCTEXT_NAMESPACE "FUnLuaEditorModule"

FUnLuaEditorToolbar::FUnLuaEditorToolbar()
	: CommandList(new FUICommandList)
{
}

void FUnLuaEditorToolbar::Initialize()
{
	BindCommands();
}

void FUnLuaEditorToolbar::BindCommands()
{
	const auto& Commands = FUnLuaEditorCommands::Get();
	CommandList->MapAction(Commands.CreateLuaTemplate, FExecuteAction::CreateRaw(this, &FUnLuaEditorToolbar::CreateLuaTemplate_Executed));
	CommandList->MapAction(Commands.CopyAsRelativePath, FExecuteAction::CreateRaw(this, &FUnLuaEditorToolbar::CopyAsRelativePath_Executed));
	CommandList->MapAction(Commands.BindToLua, FExecuteAction::CreateRaw(this, &FUnLuaEditorToolbar::BindToLua_Executed));
	CommandList->MapAction(Commands.UnbindFromLua, FExecuteAction::CreateRaw(this, &FUnLuaEditorToolbar::UnbindFromLua_Executed));
}

void FUnLuaEditorToolbar::BuildToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection(NAME_None);

	const auto Blueprint = Cast<UBlueprint>(ContextObject);
	const auto BindingStatus = GetBindingStatus(Blueprint);
	FString InStyleName;
	switch (BindingStatus)
	{
	case ELuaBindingStatus::NotBound:
		InStyleName = "UnLuaEditor.Status_NotBound";
		break;
	case ELuaBindingStatus::Bound:
		InStyleName = "UnLuaEditor.Status_Bound";
		break;
	case ELuaBindingStatus::BoundButInvalid:
		InStyleName = "UnLuaEditor.Status_BoundButInvalid";
		break;
	default:
		check(false);
	}
	UE_LOG(LogUnLua, Log, TEXT("InStyleName=%s"), *InStyleName);

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateLambda([&, BindingStatus]()
		{
			const FUnLuaEditorCommands& Commands = FUnLuaEditorCommands::Get();
			FMenuBuilder MenuBuilder(true, CommandList);
			if (BindingStatus == ELuaBindingStatus::NotBound)
			{
				MenuBuilder.AddMenuEntry(Commands.BindToLua);
			}
			else
			{
				MenuBuilder.AddMenuEntry(Commands.CopyAsRelativePath);
				MenuBuilder.AddMenuEntry(Commands.CreateLuaTemplate);
				MenuBuilder.AddMenuEntry(Commands.UnbindFromLua);
			}
			return MenuBuilder.MakeWidget();
		}),
		LOCTEXT("UnLua_Label", "UnLua"),
		LOCTEXT("UnLua_ToolTip", "UnLua"),
		FSlateIcon("UnLuaEditorStyle", *InStyleName)
	);

	ToolbarBuilder.EndSection();
}

TSharedRef<FExtender> FUnLuaEditorToolbar::GetExtender()
{
	TSharedRef<FExtender> ToolbarExtender(new FExtender());
	const auto ExtensionDelegate = FToolBarExtensionDelegate::CreateRaw(this, &FUnLuaEditorToolbar::BuildToolbar);
	ToolbarExtender->AddToolBarExtension("Debugging", EExtensionHook::After, CommandList, ExtensionDelegate);
	return ToolbarExtender;
}

void FUnLuaEditorToolbar::BindToLua_Executed() const
{
	const auto Blueprint = Cast<UBlueprint>(ContextObject);
	if (!IsValid(Blueprint))
		return;

	const auto TargetClass = Blueprint->GeneratedClass;
	if (!IsValid(TargetClass))
		return;

	if (TargetClass->ImplementsInterface(UUnLuaInterface::StaticClass()))
		return;

	const auto Ok = FBlueprintEditorUtils::ImplementNewInterface(Blueprint, FName("UnLuaInterface"));
	if (!Ok)
		return;

	const auto BlueprintEditors = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet").GetBlueprintEditors();
	for (auto BlueprintEditor : BlueprintEditors)
	{
		auto MyBlueprintEditor = static_cast<FBlueprintEditor*>(&BlueprintEditors[0].Get());
		if (!MyBlueprintEditor || MyBlueprintEditor->GetBlueprintObj() != Blueprint)
			continue;
		MyBlueprintEditor->Compile();

		const auto Func = Blueprint->GeneratedClass->FindFunctionByName(FName("GetModuleName"));
		const auto GraphToOpen = FBlueprintEditorUtils::FindScopeGraph(Blueprint, Func);
		MyBlueprintEditor->OpenGraphAndBringToFront(GraphToOpen);
	}
}

void FUnLuaEditorToolbar::UnbindFromLua_Executed() const
{
	const auto Blueprint = Cast<UBlueprint>(ContextObject);
	if (!IsValid(Blueprint))
		return;

	const auto TargetClass = Blueprint->GeneratedClass;
	if (!IsValid(TargetClass))
		return;

	if (!TargetClass->ImplementsInterface(UUnLuaInterface::StaticClass()))
		return;

	const auto Func = Blueprint->GeneratedClass->FindFunctionByName(FName("GetModuleName"));
	const auto GraphToClose = FBlueprintEditorUtils::FindScopeGraph(Blueprint, Func);

	FBlueprintEditorUtils::RemoveInterface(Blueprint, FName("UnLuaInterface"));

	const auto BlueprintEditors = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet").GetBlueprintEditors();
	for (auto BlueprintEditor : BlueprintEditors)
	{
		auto MyBlueprintEditor = static_cast<FBlueprintEditor*>(&BlueprintEditors[0].Get());
		if (!MyBlueprintEditor || MyBlueprintEditor->GetBlueprintObj() != Blueprint)
			continue;
		// MyBlueprintEditor->CloseDocumentTab(GraphToClose);
		MyBlueprintEditor->Compile();
		MyBlueprintEditor->RefreshEditors();
	}
}

void FUnLuaEditorToolbar::CreateLuaTemplate_Executed()
{
	const auto Blueprint = Cast<UBlueprint>(ContextObject);
	if (!IsValid(Blueprint))
		return;

	UClass* Class = Blueprint->GeneratedClass;
	FString ClassName = Class->GetName();
	FString OuterPath = Class->GetPathName();
	int32 LastIndex;
	if (OuterPath.FindLastChar('/', LastIndex))
	{
		OuterPath = OuterPath.Left(LastIndex + 1);
	}
	OuterPath = OuterPath.RightChop(6); // ignore "/Game/"
	FString FileName = FString::Printf(TEXT("%s%s%s.lua"), *GLuaSrcFullPath, *OuterPath, *ClassName);
	if (FPaths::FileExists(FileName))
	{
		UE_LOG(LogUnLua, Warning, TEXT("Lua file (%s) is already existed!"), *ClassName);
		return;
	}

	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("UnLua"))->GetContentDir();

	FString TemplateName;
	if (Class->IsChildOf(AActor::StaticClass()))
	{
		// default BlueprintEvents for Actor
		TemplateName = ContentDir + TEXT("/ActorTemplate.lua");
	}
	else if (Class->IsChildOf(UUserWidget::StaticClass()))
	{
		// default BlueprintEvents for UserWidget (UMG)
		TemplateName = ContentDir + TEXT("/UserWidgetTemplate.lua");
	}
	else if (Class->IsChildOf(UAnimInstance::StaticClass()))
	{
		// default BlueprintEvents for AnimInstance (animation blueprint)
		TemplateName = ContentDir + TEXT("/AnimInstanceTemplate.lua");
	}
	else if (Class->IsChildOf(UActorComponent::StaticClass()))
	{
		// default BlueprintEvents for ActorComponent
		TemplateName = ContentDir + TEXT("/ActorComponentTemplate.lua");
	}

	FString Content;
	FFileHelper::LoadFileToString(Content, *TemplateName);
	Content = Content.Replace(TEXT("TemplateName"), *ClassName);

	FFileHelper::SaveStringToFile(Content, *FileName);
}

void FUnLuaEditorToolbar::CopyAsRelativePath_Executed() const
{
	const auto Blueprint = Cast<UBlueprint>(ContextObject);
	if (!IsValid(Blueprint))
		return;

	const auto TargetClass = Blueprint->GeneratedClass;
	if (!IsValid(TargetClass))
		return;

	if (!TargetClass->ImplementsInterface(UUnLuaInterface::StaticClass()))
		return;

	const auto Func = TargetClass->FindFunctionByName(FName("GetModuleName"));
	if (!IsValid(Func))
		return;

	FString ModuleName;
	auto DefaultObject = TargetClass->GetDefaultObject();
	DefaultObject->UObject::ProcessEvent(Func, &ModuleName);

	const auto RelativePath = ModuleName.Replace(TEXT("."), TEXT("/")) + TEXT(".lua");
	FPlatformMisc::ClipboardCopy(*RelativePath);
}

#undef LOCTEXT_NAMESPACE
