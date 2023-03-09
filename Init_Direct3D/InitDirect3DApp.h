#pragma once

#include "D3dHeader.h"
#include "D3dApp.h"
#include <DirectXColors.h>
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/DDSTextureLoader.h"
#include "../Common/Camera.h"
#include "ShadowMap.h"
#include "LoadM3d.h"
#include "SkinnedData.h"

class InitDirect3DApp : public D3DApp
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp();

	virtual bool Initialize()override;

private:
	virtual void CreateDsvDescriptorHeaps()override;

	virtual void OnResize()override;
	
	virtual void Update(const GameTimer& gt)override;
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdatePassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt)override;
	void DrawRenderItems(const std::vector<RenderItem*>& ritems);
	void DrawSceneToShadowMap();

	virtual void DrawBegin(const GameTimer& gt)override;
	virtual void DrawEnd(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

private:
	// Skinned Model 로드
	void LoadSkinnedModel();

	// 텍스처 로드
	void LoadTextures();

	// SRV 서술자 설정
	void BuildDescriptorHeaps();

	// 기하 도형 생성
	void BuildBoxGeometry();
	void BuildGridGeometry();
	void BuildSphereGeometry();
	void BuildCylinderGeometry();
	void BuildQuadGeometry();
	void BuildSkullGeometry();

	// 재질 생성
	void BuildMaterials();

	// 렌더링 할 아이템 생성
	void BuildRenderItems();

	void BuildInputLayout();
	void BuildShaders();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> GetStaticSamplers();

private:
	// 입력 배치
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	// 스키닝 애니메이션용 입력 배치
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	// 개별 오브젝트 상수 버퍼
	ComPtr<ID3D12Resource>	mObjectCB = nullptr;
	BYTE* mObjectMappedData = nullptr;
	UINT mObjectByteSize = 0;

	// 개별 오브젝트 재질 상수 버퍼
	ComPtr<ID3D12Resource>	mMaterialCB = nullptr;
	BYTE* mMaterialMappedData = nullptr;
	UINT mMaterialByteSize = 0;

	// 공용 상수 버퍼
	ComPtr<ID3D12Resource>	mPassCB = nullptr;
	BYTE* mPassMappedData = nullptr;
	UINT mPassByteSize = 0;

	// Bone Transform 버퍼
	ComPtr<ID3D12Resource>	mSkinnedCB = nullptr;
	BYTE* mSkinnedMappedData = nullptr;
	UINT mSkinnedByteSize = 0;

	// 루트 시그니처
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	// 쉐이더 맵
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	// 렌더링파이프라인 스테이트 맵
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	// 서술자 힙
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	UINT mCbvSrvDescriptorSize = 0;

	// 렌더링 할 오브젝트 리스트
	std::vector<std::unique_ptr<RenderItem>> mRenderitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	// 기하 구조 맵
	std::unordered_map<std::string, std::unique_ptr<GeometryInfo>> mGeometries;

	// 재질 구조 맵
	std::unordered_map<std::string, std::unique_ptr<MaterialInfo>> mMaterials;

	// 텍스처 맵
	std::unordered_map<std::string, std::unique_ptr<TextureInfo>> mTextures;
	
	// 그림자 맵 
	std::unique_ptr<ShadowMap> mShadowMap;

	// 스카이박스 텍스처 인덱스
	UINT mSkyboxTexHeapIndex = 0;

	// 그림자 맵 텍스처 인덱스
	UINT mShadowMapHeapIndex = 0;

	// 그림자 맵 서술자
	CD3DX12_GPU_DESCRIPTOR_HANDLE mShadowMapSrv;

	// Skinned Model Data
	UINT mSkinnedSrvHeapStart = 0;
	std::string mSkinnedModelFilename = "..\\Models\\soldier.m3d";
	SkinnedData mSkinnedInfo;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;

	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	// 경계 구
	DirectX::BoundingSphere mSceneBounds;

	// 라이트 관련 행렬
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirection = XMFLOAT3(0.57735f, -0.57735f, 0.57735);
	XMFLOAT3 mRotatedLightDirection;

	// 카메라 클래스
	Camera mCamera;

	// 마우스 좌표
	POINT mLastMousePos = { 0, 0 };
};