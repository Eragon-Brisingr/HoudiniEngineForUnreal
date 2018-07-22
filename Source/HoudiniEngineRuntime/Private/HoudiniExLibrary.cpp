// Fill out your copyright notice in the Description page of Project Settings.

#include "HoudiniApi.h"
#include "HoudiniExLibrary.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include <FoliageType.h>
#include "HoudiniAssetInstanceInput.h"
#include <InstancedFoliageActor.h>
#include <InstancedFoliage.h>

int32 HoudiniExLibrary::FoliageTypeIndex = 0;

bool HoudiniExLibrary::HapiCreateInputNodeForFoliageType(class UFoliageType* FoliageType, HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& OutCreatedNodeIds, class UStaticMeshComponent* StaticMeshComponent /*= nullptr*/, const bool& ExportAllLODs /*= false*/, const bool& ExportSockets /*= false*/)
{
	UStaticMesh* Mesh = FoliageType->GetStaticMesh();
	if (FHoudiniEngineUtils::HapiCreateInputNodeForStaticMesh(Mesh, ConnectedAssetId, OutCreatedNodeIds, StaticMeshComponent, ExportAllLODs, ExportSockets))
	{
		HAPI_AttributeInfo AttributeInfo;
		FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		FString Name_MeshPath = TEXT("MeshPath") + FString::FromInt(HoudiniExLibrary::FoliageTypeIndex);
		FString Name_FoliagePath = TEXT("FoliagePath") + FString::FromInt(HoudiniExLibrary::FoliageTypeIndex);
		HoudiniExLibrary::FoliageTypeIndex += 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
			0, TCHAR_TO_ANSI(*Name_MeshPath), &AttributeInfo), false);

		TArray<const char*> MeshPath{ TCHAR_TO_ANSI(*Mesh->GetPathName()) };

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			ConnectedAssetId, 0, TCHAR_TO_ANSI(*Name_MeshPath), &AttributeInfo,
			MeshPath.GetData(), 0, AttributeInfo.count), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
			0, TCHAR_TO_ANSI(*Name_FoliagePath), &AttributeInfo), false);

		TArray<const char*> FoliagePath{ TCHAR_TO_ANSI(*FoliageType->GetPathName()) };

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			ConnectedAssetId, 0, TCHAR_TO_ANSI(*Name_FoliagePath), &AttributeInfo,
			FoliagePath.GetData(), 0, AttributeInfo.count), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), ConnectedAssetId), false);

		return true;
	}
	return false;
}

bool HoudiniExLibrary::CreateInstanceFoliageInput(class UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput, HAPI_NodeId AssetId, FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	TArray<FString> FoliagePaths;
	HAPI_AttributeInfo ResultAttributeInfo;
	FMemory::Memzero< HAPI_AttributeInfo >(ResultAttributeInfo);
	if (HoudiniGeoPartObject.HapiGetAttributeDataAsString(AssetId, FoliageAttributeName, HAPI_ATTROWNER_POINT, ResultAttributeInfo, FoliagePaths))
	{
		TMap<FString, UFoliageType*> FoliagePathToObject;
		for (FString FoliagePath : FoliagePaths)
		{
			UFoliageType* FoliageObject = (UFoliageType*)StaticLoadObject(UFoliageType::StaticClass(), nullptr, *FoliagePath, nullptr, LOAD_None, nullptr);
			FoliagePathToObject.Add(FoliagePath, FoliageObject);
		}

		TArray< FTransform > AllTransforms;
		HoudiniGeoPartObject.HapiGetInstanceTransforms(AssetId, AllTransforms);

		UWorld* World = GEditor->GetEditorWorldContext().World();
		ULevel* TargetLevel = World->GetCurrentLevel();

		for (auto Iter : FoliagePathToObject)
		{
			const FString& FoliagePath = Iter.Key;
			UFoliageType* FoliageType = Iter.Value;

			if (FoliageType)
			{
				AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(TargetLevel, true);
				FFoliageMeshInfo* MeshInfo;
				IFA->RemoveFoliageType(&FoliageType, 1);
				UFoliageType* FoliageSettings = IFA->AddFoliageType(FoliageType, &MeshInfo);

				for (int32 Idx = 0; Idx < AllTransforms.Num(); ++Idx)
				{
					if (FoliagePath == FoliagePaths[Idx])
					{
						FTransform InstanceTransform = AllTransforms[Idx];
						FFoliageInstance Inst;
						Inst.Location = InstanceTransform.GetLocation();
						Inst.Rotation = InstanceTransform.GetRotation().Rotator();
						MeshInfo->AddInstance(IFA, FoliageSettings, Inst, nullptr, true);
					}
				}
			}
		}

		// 							GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Foliage);
		// 							FEdModeFoliage* FoliageEditMode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

		return true;
	}
	return false;
}
