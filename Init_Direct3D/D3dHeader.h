#pragma once

#include "SkinnedData.h"
#include "../Common/d3dUtil.h"

using namespace DirectX;
using namespace Microsoft::WRL;

#define MAXLIGHTS 16

enum class RenderLayer : int
{
	Opaque = 0,
	SkinnedOpaque,
	Transparent,
	AlphaTested,
	Debug,
	Skybox,
	Count
};

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 Uv;
	XMFLOAT3 Tangent;
};


struct SkinnedVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 Uv;
	XMFLOAT3 Tangent;
	XMFLOAT3 BoneWeights;
	BYTE BoneIndices[4];
};


// ���� ������Ʈ ���
struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

// ���� ������Ʈ�� ���� ���
struct MatConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	UINT Texture_On = 0;
	UINT Normal_On = 0;
	XMFLOAT2 padding = { 0.0f, 0.0f };
};

// ���� ���� ����ü
struct LightInfo
{
	UINT LightTyp = 0;
	XMFLOAT3 padding = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
	float FalloffEnd = 10.0f;
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
	float SpotPower = 64.0f;
};

// ���� ����ϴ� ���
struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();

	// ���� ����
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	UINT LightCount = MAXLIGHTS;
	LightInfo Lights[MAXLIGHTS];

	// �Ȱ� ȿ�� ����
	XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float gFogStart = 5.0f;
	float gFogRange = 150.0f;
	XMFLOAT2 FogPadding;
};


// BoneTransform ��� ����
struct SkinnedConstants
{
	XMFLOAT4X4 BoneTransforms[96];
};

struct GeometryInfo
{
	std::string Name;

	// ���� ���� ��
	D3D12_VERTEX_BUFFER_VIEW                VertexView = { };
	ComPtr<ID3D12Resource>                  VertexBuffer = nullptr;

	// �ε��� ���� ��
	D3D12_INDEX_BUFFER_VIEW                 IndexView = { };
	ComPtr<ID3D12Resource>                  IndexBuffer = nullptr;

	// ������ ����
	int VertexCount = 0;
	// �ε����� ����
	int IndexCount = 0;

	// �ε��� ����
	UINT StartIndexLocation = 0;
	// ���ؽ� ����
	int BaseVertexLocation = 0;
};

// �ؽ�ó ����ü
struct TextureInfo
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

// ���� ����ü
struct MaterialInfo
{
	std::string Name;

	int MatCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int NormalSrvHeapIndex = -1;

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
};

struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<XMFLOAT4X4> FinalTransforms;
	std::string ClipName;
	float TimePos = 0.0f;

	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
			TimePos = 0.0f;

		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

// ������Ʈ ����ü
struct RenderItem
{
	RenderItem() = default;

	UINT ObjCBIndex = -1;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// ���� ����
	GeometryInfo* Geo = nullptr;
	MaterialInfo* Mat = nullptr;

	UINT SkinnedCBIndex = 0;
	SkinnedModelInstance* SkinnedModelInst = nullptr;
};