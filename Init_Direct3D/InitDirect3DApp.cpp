#include "InitDirect3DApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        InitDirect3DApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();

    if(mObjectCB)
        mObjectCB->Unmap(0, nullptr);
    if(mMaterialCB)
        mMaterialCB->Unmap(0, nullptr);
    if(mPassCB)
        mPassCB->Unmap(0, nullptr);
}

bool InitDirect3DApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // �ʱ�ȭ ��ɵ��� �غ��ϱ� ���� ��� ����� �缳�� �Ѵ�.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // -----------------------------------------------------------
    // �ʱ�ȭ ��ɵ�
    // -----------------------------------------------------------
    // ��豸 ����
    mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    mSceneBounds.Radius = sqrtf(10.0f * 10.f + 15.0f * 15.0f);

    // �׸��� �� ����
    mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);

    // ī�޶� �ʱ� ��ġ ����
    mCamera.SetPosition(0.0f, 2.0f, -15.0f);

    // Skinned Model �ε�
    LoadSkinnedModel();

    // �ؽ�ó �ε�
    LoadTextures();

    // SRV ������ ����
    BuildDescriptorHeaps();

    // ���� ���� ����
    BuildBoxGeometry();
    BuildGridGeometry();
    BuildSphereGeometry();
    BuildCylinderGeometry();
    BuildQuadGeometry();
    BuildSkullGeometry();

    // ���� ����
    BuildMaterials();

    // �������� ������Ʈ ����
    BuildRenderItems();

    // ������ ����
    BuildInputLayout();
    BuildShaders();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildPSO();

    // �ʱ�ȭ ��ɵ��� �����Ѵ�.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // �ʱ�ȭ�� �Ϸ� �� ������ ��ٸ���.
    FlushCommandQueue();

    return true;
}

void InitDirect3DApp::CreateDsvDescriptorHeaps()
{
    // Add +1 DSV for shadow map.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;

    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesv;
    dsvDesv.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesv.Format = mDepthStencilFormat;
    dsvDesv.Texture2D.MipSlice = 0;

    mDsvView = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesv, mDsvView);
}

void InitDirect3DApp::OnResize()
{
    D3DApp::OnResize();

    // â�� ũ�Ⱑ �ٲ�����Ƿ� ��Ⱦ�� �����ϰ� ���� ����� �ٽ� ����Ѵ�.
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void InitDirect3DApp::Update(const GameTimer& gt)
{
    UpdateCamera(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateShadowTransform(gt);
    UpdatePassCB(gt);
    UpdateShadowPassCB(gt);
    UpdateSkinnedCBs(gt);
}

void InitDirect3DApp::UpdateCamera(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(10.0f * dt);

    if (GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-10.0f * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-10.0f * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(10.0f * dt);

    mCamera.UpdateViewMatrix();
}

void InitDirect3DApp::UpdateObjectCBs(const GameTimer& gt)
{
    for (auto& e : mRenderitems)
    {
        XMMATRIX world = XMLoadFloat4x4(&e->World);
        XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

        // ���� ������Ʈ ��� ���� ����
        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

        XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

        UINT elementIndex = e->ObjCBIndex;
        UINT elementByteSize = (sizeof(ObjectConstants) + 255) & ~255;
        memcpy(&mObjectMappedData[elementIndex * elementByteSize], &objConstants, sizeof(ObjectConstants));
    }
}

void InitDirect3DApp::UpdateMaterialCBs(const GameTimer& gt)
{
    for (auto& e : mMaterials)
    {
        MaterialInfo* mat = e.second.get();

        MatConstants matConstants;
        matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
        matConstants.FresnelR0 = mat->FresnelR0;
        matConstants.Roughness = mat->Roughness;
        matConstants.Texture_On = (mat->DiffuseSrvHeapIndex == -1) ? 0 : 1;
        matConstants.Normal_On = (mat->NormalSrvHeapIndex == -1) ? 0 : 1;

        UINT elementIndex = mat->MatCBIndex;
        UINT elementByteSize = (sizeof(MatConstants) + 255) & ~255;
        memcpy(&mMaterialMappedData[elementIndex * elementByteSize], &matConstants, sizeof(MatConstants));
    }
}

void InitDirect3DApp::UpdateShadowTransform(const GameTimer& gt)
{
    // ���� ȸ��
    mLightRotationAngle += 0.1f * gt.DeltaTime();

    XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
    XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirection);
    lightDir = XMVector3TransformNormal(lightDir, R);
    XMStoreFloat3(&mRotatedLightDirection, lightDir);

    // ���� ���� ���
    XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
    XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
    XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

    XMFLOAT3 sphereCenterLS;
    XMStoreFloat3(&sphereCenterLS, XMVector3Transform(targetPos, lightView));

    float l = sphereCenterLS.x - mSceneBounds.Radius;
    float b = sphereCenterLS.y - mSceneBounds.Radius;
    float n = sphereCenterLS.z - mSceneBounds.Radius;
    float r = sphereCenterLS.x + mSceneBounds.Radius;
    float t = sphereCenterLS.y + mSceneBounds.Radius;
    float f = sphereCenterLS.z + mSceneBounds.Radius;

    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

    // NDC space [ -1, +1 ]^2 -> texture space [ 0, 1 ]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    XMMATRIX S = lightView * lightProj * T;

    mLightNearZ = n;
    mLightFarZ = f;
    XMStoreFloat3(&mLightPosW, lightPos);
    XMStoreFloat4x4(&mLightView, lightView);
    XMStoreFloat4x4(&mLightProj, lightProj);
    XMStoreFloat4x4(&mShadowTransform, S);
}

void InitDirect3DApp::UpdatePassCB(const GameTimer& gt)
{
    PassConstants mainPass;
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invviewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

    XMStoreFloat4x4(&mainPass.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mainPass.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mainPass.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mainPass.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mainPass.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mainPass.InvViewProj, XMMatrixTranspose(invviewProj));
    XMStoreFloat4x4(&mainPass.ShadowTransform, XMMatrixTranspose(shadowTransform));

    mainPass.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mainPass.EyePosW = mCamera.GetPosition3f();
    mainPass.LightCount = 1;

    mainPass.Lights[0].LightTyp = 0;
    mainPass.Lights[0].Direction = mRotatedLightDirection;
    mainPass.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };

    memcpy(&mPassMappedData[0], &mainPass, sizeof(PassConstants));
}

void InitDirect3DApp::UpdateShadowPassCB(const GameTimer& gt)
{
    PassConstants shadowPass;
    XMMATRIX view = XMLoadFloat4x4(&mLightView);
    XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invviewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&shadowPass.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&shadowPass.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&shadowPass.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&shadowPass.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&shadowPass.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&shadowPass.InvViewProj, XMMatrixTranspose(invviewProj));
    shadowPass.EyePosW = mLightPosW;

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
    memcpy(&mPassMappedData[passCBByteSize], &shadowPass, sizeof(PassConstants));
}

void InitDirect3DApp::UpdateSkinnedCBs(const GameTimer& gt)
{
    mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

    SkinnedConstants skinnedCB;
    std::copy(
        std::begin(mSkinnedModelInst->FinalTransforms),
        std::end(mSkinnedModelInst->FinalTransforms),
        &skinnedCB.BoneTransforms[0]);

    memcpy(&mSkinnedMappedData[0], &skinnedCB, sizeof(SkinnedConstants));
}

void InitDirect3DApp::Draw(const GameTimer& gt)
{
    // ������ ������ ���������ο� ����
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // ��Ʈ �ñ״�ó�� ��� ���� ���� �����Ѵ�.
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // �׸��� �� ����
    DrawSceneToShadowMap();

    // ������Ʈ ������
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // ���� ��� ���� �並 ����
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mPassCB->GetGPUVirtualAddress();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);
    
    // ��ī�̹ڽ� �ؽ�ó ����
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    skyTexDescriptor.Offset(mSkyboxTexHeapIndex, mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);

    mCommandList->SetGraphicsRootDescriptorTable(6, mShadowMapSrv);

    // to do : Rendering   
    mCommandList->SetPipelineState(mPSOs["opaque"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

    mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::AlphaTested]);

    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::Transparent]);

    mCommandList->SetPipelineState(mPSOs["debug"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::Debug]);

    mCommandList->SetPipelineState(mPSOs["skybox"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::Skybox]);
}

void InitDirect3DApp::DrawRenderItems(const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = (sizeof(ObjectConstants) + 255) & ~255;
    UINT matCBByteSize = (sizeof(MaterialConstants) + 255) & ~255;
    UINT skinnedCBByteSize = (sizeof(SkinnedConstants) + 255) & ~255;

    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        if (ri->Geo == nullptr)
            continue;

        // ���� ������Ʈ ��� ���� �� ����
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mObjectCB->GetGPUVirtualAddress();
        objCBAddress += ri->ObjCBIndex * objCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        // ���� ������Ʈ ���� ��� ���� �� ����
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = mMaterialCB->GetGPUVirtualAddress();
        matCBAddress += ri->Mat->MatCBIndex * matCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(1, matCBAddress);

        // �ؽ�ó ���� ������ ����
        if (ri->Mat->DiffuseSrvHeapIndex != -1)
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

            mCommandList->SetGraphicsRootDescriptorTable(4, tex);
        }

        // �븻 �ؽ�ó ���� ������ ����
        if (ri->Mat->NormalSrvHeapIndex != -1)
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            tex.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);

            mCommandList->SetGraphicsRootDescriptorTable(5, tex);
        }

        // Bone Transform ��� ����
        if (ri->SkinnedModelInst != nullptr)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = mSkinnedCB->GetGPUVirtualAddress();
            skinnedCBAddress += ri->SkinnedCBIndex * skinnedCBByteSize;
            mCommandList->SetGraphicsRootConstantBufferView(7, skinnedCBAddress);
        }
        else
        {
            mCommandList->SetGraphicsRootConstantBufferView(7, 0);
        }


        // ����, �ε���, �������� ����
        mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexView);
        mCommandList->IASetIndexBuffer(&ri->Geo->IndexView);
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        // ������
        mCommandList->DrawIndexedInstanced(
            ri->Geo->IndexCount, 
            1, 
            ri->Geo->StartIndexLocation, 
            ri->Geo->BaseVertexLocation, 
            0);
    }
}

void InitDirect3DApp::DrawSceneToShadowMap()
{
    // ������Ʈ ������
    mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
    mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

    // �׸��� �� ���� ��� ���� �� ����
    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mPassCB->GetGPUVirtualAddress() + passCBByteSize;
    mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

    mCommandList->SetPipelineState(mPSOs["shadow"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["skinnedShadow"].Get());
    DrawRenderItems(mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void InitDirect3DApp::DrawBegin(const GameTimer& gt)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
}

void InitDirect3DApp::DrawEnd(const GameTimer& gt)
{
    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void InitDirect3DApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void InitDirect3DApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void InitDirect3DApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // ���콺 �� �ȼ� �̵��� 4���� 1���� ������Ų��.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void InitDirect3DApp::LoadSkinnedModel()
{
    std::vector<M3DLoader::SkinnedVertex> vertices;
    std::vector<std::uint16_t> indices;

    M3DLoader m3dLoader;
    m3dLoader.LoadM3d(mSkinnedModelFilename, vertices, indices, mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);

    mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
    mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
    mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
    mSkinnedModelInst->ClipName = "Take1";
    mSkinnedModelInst->TimePos = 0.0f;

    for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
    {
        // ���� ������ �Է�
        auto geo = std::make_unique<GeometryInfo>();
        geo->Name = "sm_" + std::to_string(i);

        // ���� ���� �� ��
        geo->VertexCount = (UINT)vertices.size();
        const UINT vbByteSize = geo->VertexCount * sizeof(SkinnedVertex);

        D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

        md3dDevice->CreateCommittedResource(
            &heapProperty,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&geo->VertexBuffer));

        void* vertexDataBuff = nullptr;
        CD3DX12_RANGE vertexRange(0, 0);
        geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
        memcpy(vertexDataBuff, vertices.data(), vbByteSize);
        geo->VertexBuffer->Unmap(0, nullptr);

        geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
        geo->VertexView.StrideInBytes = sizeof(SkinnedVertex);
        geo->VertexView.SizeInBytes = vbByteSize;

        // �ε��� ���� �� ��
        geo->IndexCount = (UINT)indices.size();
        const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

        heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

        md3dDevice->CreateCommittedResource(
            &heapProperty,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&geo->IndexBuffer));

        void* indexDataBuff = nullptr;
        CD3DX12_RANGE indexRange(0, 0);
        geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
        memcpy(indexDataBuff, indices.data(), ibByteSize);
        geo->IndexBuffer->Unmap(0, nullptr);

        geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
        geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
        geo->IndexView.SizeInBytes = ibByteSize;

        geo->IndexCount = mSkinnedSubsets[i].FaceCount * 3;
        geo->StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
        geo->BaseVertexLocation = 0;

        mGeometries[geo->Name] = std::move(geo);
    }   
}

void InitDirect3DApp::LoadTextures()
{
    std::vector<std::string> texNames =
    {
        "bricks",           // 0
        "bricksNormal",     // 1
        "stone",            // 2
        "tile",             // 3
        "tileNormal",       // 4
        "fence",            // 5
        "default",          // 6
        "skyCubeMap",       // 7
    };

    std::vector<std::wstring> texFileNames =
    {
        L"../Textures/bricks.dds",
        L"../Textures/bricks_nmap.dds",
        L"../Textures/stone.dds",
        L"../Textures/tile.dds",
        L"../Textures/tile_nmap.dds",
        L"../Textures/WireFence.dds",
        L"../Textures/white1x1.dds",
        L"../Textures/grasscube1024.dds",
    };

    // Skinned Model Texture add
    for (UINT i = 0; i < (UINT)mSkinnedMats.size(); ++i)
    {
        std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
        std::string normalName = mSkinnedMats[i].NormalMapName;

        std::wstring diffuseFilename = L"../Textures/" + AnsiToWString(diffuseName);
        std::wstring normalFilename = L"../Textures/" + AnsiToWString(normalName);

        diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));
        normalName = normalName.substr(0, normalName.find_last_of("."));

        if (std::find(mSkinnedTextureNames.begin(), mSkinnedTextureNames.end(), diffuseName) == mSkinnedTextureNames.end())
        {
            mSkinnedTextureNames.push_back(diffuseName);
            texNames.push_back(diffuseName);
            texFileNames.push_back(diffuseFilename);
        }

        if (std::find(mSkinnedTextureNames.begin(), mSkinnedTextureNames.end(), normalName) == mSkinnedTextureNames.end())
        {
            mSkinnedTextureNames.push_back(normalName);
            texNames.push_back(normalName);
            texFileNames.push_back(normalFilename);
        }
    }

    for (int i = 0; i < (int)texFileNames.size(); ++i)
    {
        auto texMap = std::make_unique<TextureInfo>();
        texMap->Name = texNames[i];
        texMap->Filename = texFileNames[i];
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
            mCommandList.Get(), texMap->Filename.c_str(),
            texMap->Resource, texMap->UploadHeap));
        
        mTextures[texMap->Name] = std::move(texMap);
    }
}

void InitDirect3DApp::BuildBoxGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);

    // ���� ����
    std::vector<Vertex> vertices(box.Vertices.size());

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].Uv = box.Vertices[i].TexC;
        vertices[k].Tangent = box.Vertices[i].TangentU;
    }

    // �ε��� ����
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Box";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildGridGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);

    // ���� ����
    std::vector<Vertex> vertices(grid.Vertices.size());

    UINT k = 0;
    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].Uv = grid.Vertices[i].TexC;
        vertices[k].Tangent = grid.Vertices[i].TangentU;
    }

    // �ε��� ����
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Grid";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildSphereGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);

    // ���� ����
    std::vector<Vertex> vertices(sphere.Vertices.size());

    UINT k = 0;
    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].Uv = sphere.Vertices[i].TexC;
        vertices[k].Tangent = sphere.Vertices[i].TangentU;
    }

    // �ε��� ����
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Sphere";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildCylinderGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    // ���� ����
    std::vector<Vertex> vertices(cylinder.Vertices.size());

    UINT k = 0;
    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].Uv = cylinder.Vertices[i].TexC;
        vertices[k].Tangent = cylinder.Vertices[i].TangentU;
    }

    // �ε��� ����
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Cylinder";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildQuadGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    // ���� ����
    std::vector<Vertex> vertices(quad.Vertices.size());

    UINT k = 0;
    for (size_t i = 0; i < quad.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = quad.Vertices[i].Position;
        vertices[k].Normal = quad.Vertices[i].Normal;
        vertices[k].Uv = quad.Vertices[i].TexC;
        vertices[k].Tangent = quad.Vertices[i].TangentU;
    }

    // �ε��� ����
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Quad";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildSkullGeometry()
{
    std::ifstream fin("../Models/skull.txt");
    if (!fin)
    {
        MessageBox(0, L"../Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vCount = 0;
    UINT tCount = 0;

    std::string ignore;

    fin >> ignore >> vCount;
    fin >> ignore >> tCount;
    fin >> ignore >> ignore >> ignore >> ignore;

    std::vector<Vertex> vertices(vCount);
    for (UINT i = 0; i < vCount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
    }

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(tCount * 3);
    for (UINT i = 0; i < tCount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    // ���� ������ �Է�
    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Skull";

    // ���� ���� �� ��
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuff = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuff);
    memcpy(vertexDataBuff, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexView.StrideInBytes = sizeof(Vertex);
    geo->VertexView.SizeInBytes = vbByteSize;

    // �ε��� ���� �� ��
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::int32_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuff = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuff);
    memcpy(indexDataBuff, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexView.Format = DXGI_FORMAT_R32_UINT;
    geo->IndexView.SizeInBytes = ibByteSize;

    mGeometries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildMaterials()
{
    auto bricks0 = std::make_unique<MaterialInfo>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = 0;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->NormalSrvHeapIndex = 1;
    bricks0->DiffuseAlbedo = XMFLOAT4(Colors::White);
    bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks0->Roughness = 0.1f;
    mMaterials[bricks0->Name] = std::move(bricks0);

    auto stone0 = std::make_unique<MaterialInfo>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 1;
    stone0->DiffuseSrvHeapIndex = 2;
    stone0->DiffuseAlbedo = XMFLOAT4(Colors::White);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;
    mMaterials[stone0->Name] = std::move(stone0);

    auto tile0 = std::make_unique<MaterialInfo>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = 2;
    tile0->DiffuseSrvHeapIndex = 3;
    tile0->NormalSrvHeapIndex = 4;
    tile0->DiffuseAlbedo = XMFLOAT4(Colors::White);
    tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tile0->Roughness = 0.2f;
    mMaterials[tile0->Name] = std::move(tile0);

    auto skull = std::make_unique<MaterialInfo>();
    skull->Name = "skull";
    skull->MatCBIndex = 3;
    skull->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
    skull->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skull->Roughness = 0.3f;
    mMaterials[skull->Name] = std::move(skull);

    auto wirefence = std::make_unique<MaterialInfo>();
    wirefence->Name = "wirefence";
    wirefence->MatCBIndex = 4;
    wirefence->DiffuseSrvHeapIndex = 5;
    wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    wirefence->Roughness = 0.25f;
    mMaterials[wirefence->Name] = std::move(wirefence);

    auto mirror = std::make_unique<MaterialInfo>();
    mirror->Name = "mirror";
    mirror->MatCBIndex = 5;
    mirror->DiffuseSrvHeapIndex = 6;
    mirror->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mirror->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    mirror->Roughness = 0.1f;
    mMaterials[mirror->Name] = std::move(mirror);

    auto skybox = std::make_unique<MaterialInfo>();
    skybox->Name = "skybox";
    skybox->MatCBIndex = 6;
    skybox->DiffuseSrvHeapIndex = mSkyboxTexHeapIndex;
    skybox->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skybox->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    skybox->Roughness = 1.0f;
    mMaterials[skybox->Name] = std::move(skybox);

    UINT matCBIndex = 7;
    UINT srvHeapIndex = mSkinnedSrvHeapStart;
    for (UINT i = 0; i < (UINT)mSkinnedMats.size(); ++i)
    {
        auto mat = std::make_unique<MaterialInfo>();
        mat->Name = mSkinnedMats[i].Name;
        mat->MatCBIndex = matCBIndex++;
        mat->DiffuseSrvHeapIndex = srvHeapIndex++;
        mat->NormalSrvHeapIndex = srvHeapIndex++;
        mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
        mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
        mat->Roughness = mSkinnedMats[i].Roughness;

        mMaterials[mat->Name] = std::move(mat);
    }
}

void InitDirect3DApp::BuildRenderItems()
{
    // skybox
    auto skyRItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRItem->World, XMMatrixScaling(5000.f, 5000.f, 5000.f));
    skyRItem->TexTransform = MathHelper::Identity4x4();
    skyRItem->ObjCBIndex = 0;
    skyRItem->Geo = mGeometries["Sphere"].get();
    skyRItem->Mat = mMaterials["skybox"].get();
    skyRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mRitemLayer[(int)RenderLayer::Skybox].push_back(skyRItem.get());
    mRenderitems.push_back(std::move(skyRItem));

    auto boxRItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRItem->World, XMMatrixScaling(2.f, 2.f, 2.f) * XMMatrixTranslation(0.f, 0.5f, 0.f));
    XMStoreFloat4x4(&boxRItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    boxRItem->ObjCBIndex = 1;
    boxRItem->Geo = mGeometries["Box"].get();
    boxRItem->Mat = mMaterials["wirefence"].get();
    boxRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRItem.get());
    mRenderitems.push_back(std::move(boxRItem));


    auto gridRItem = std::make_unique<RenderItem>();
    gridRItem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRItem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
    gridRItem->ObjCBIndex = 2;
    gridRItem->Geo = mGeometries["Grid"].get();
    gridRItem->Mat = mMaterials["tile0"].get();
    gridRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRItem.get());
    mRenderitems.push_back(std::move(gridRItem));


    auto skullRItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skullRItem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.f, 1.f, 0.f));
    skullRItem->ObjCBIndex = 3;
    skullRItem->Geo = mGeometries["Skull"].get();
    skullRItem->Mat = mMaterials["skull"].get();
    skullRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRItem.get());
    mRenderitems.push_back(std::move(skullRItem));

    auto quadRItem = std::make_unique<RenderItem>();
    quadRItem->World = MathHelper::Identity4x4();
    quadRItem->TexTransform = MathHelper::Identity4x4();
    quadRItem->ObjCBIndex = 4;
    quadRItem->Geo = mGeometries["Quad"].get();
    quadRItem->Mat = mMaterials["tile0"].get();
    quadRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mRitemLayer[(int)RenderLayer::Debug].push_back(quadRItem.get());
    mRenderitems.push_back(std::move(quadRItem));

    XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    UINT objectCBIndex = 5;
    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRItem = std::make_unique<RenderItem>();
        auto rightCylRItem = std::make_unique<RenderItem>();
        auto leftSpRItem = std::make_unique<RenderItem>();
        auto rightSpRItem = std::make_unique<RenderItem>();

        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.f, 1.5f, -10.f + i * 5.f);
        XMMATRIX rightCylWorld = XMMatrixTranslation(5.f, 1.5f, -10.f + i * 5.f);
        XMMATRIX leftSpWorld = XMMatrixTranslation(-5.f, 3.5f, -10.f + i * 5.f);
        XMMATRIX rightSpWorld = XMMatrixTranslation(5.f, 3.5f, -10.f + i * 5.f);

        XMStoreFloat4x4(&leftCylRItem->World, leftCylWorld);
        XMStoreFloat4x4(&leftCylRItem->TexTransform, brickTexTransform);
        leftCylRItem->ObjCBIndex = objectCBIndex++;
        leftCylRItem->Geo = mGeometries["Cylinder"].get();
        leftCylRItem->Mat = mMaterials["bricks0"].get();
        leftCylRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRItem.get());
        mRenderitems.push_back(std::move(leftCylRItem));

        XMStoreFloat4x4(&rightCylRItem->World, rightCylWorld);
        XMStoreFloat4x4(&rightCylRItem->TexTransform, brickTexTransform);
        rightCylRItem->ObjCBIndex = objectCBIndex++;
        rightCylRItem->Geo = mGeometries["Cylinder"].get();
        rightCylRItem->Mat = mMaterials["bricks0"].get();
        rightCylRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRItem.get());
        mRenderitems.push_back(std::move(rightCylRItem));

        XMStoreFloat4x4(&leftSpRItem->World, leftSpWorld);
        leftSpRItem->TexTransform = MathHelper::Identity4x4();
        leftSpRItem->ObjCBIndex = objectCBIndex++;
        leftSpRItem->Geo = mGeometries["Sphere"].get();
        leftSpRItem->Mat = mMaterials["mirror"].get();
        leftSpRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSpRItem.get());
        mRenderitems.push_back(std::move(leftSpRItem));

        XMStoreFloat4x4(&rightSpRItem->World, rightSpWorld);
        rightSpRItem->TexTransform = MathHelper::Identity4x4();
        rightSpRItem->ObjCBIndex = objectCBIndex++;
        rightSpRItem->Geo = mGeometries["Sphere"].get();
        rightSpRItem->Mat = mMaterials["mirror"].get();
        rightSpRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSpRItem.get());
        mRenderitems.push_back(std::move(rightSpRItem));
    }

    for (UINT i = 0; i < (UINT)mSkinnedMats.size(); ++i)
    {
        std::string meshName = "sm_" + std::to_string(i);

        auto ritem = std::make_unique<RenderItem>();
        XMMATRIX scale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
        XMMATRIX rotate = XMMatrixRotationY(MathHelper::Pi);
        XMMATRIX pos = XMMatrixTranslation(0.0f, 0.0f, -5.0f);
        XMStoreFloat4x4(&ritem->World, scale * rotate * pos);

        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = objectCBIndex++;
        ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
        ritem->Geo = mGeometries[meshName].get();
        ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        ritem->SkinnedCBIndex = 0;
        ritem->SkinnedModelInst = mSkinnedModelInst.get();
        mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
        mRenderitems.push_back(std::move(ritem));
    }

}

void InitDirect3DApp::BuildInputLayout()
{
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    mSkinnedInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void InitDirect3DApp::BuildShaders()
{
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO skinnedDefines[] =
    {
        "SKINNED", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Color.hlsl", skinnedDefines, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Color.hlsl", defines, "PS", "ps_5_0");
    mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Color.hlsl", alphaTestDefines, "PS", "ps_5_0");

    mShaders["skyboxVS"] = d3dUtil::CompileShader(L"Skybox.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["skyboxPS"] = d3dUtil::CompileShader(L"Skybox.hlsl", nullptr, "PS", "ps_5_0");

    mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["skinnedshadowVS"] = d3dUtil::CompileShader(L"Shadow.hlsl", skinnedDefines, "VS", "vs_5_0");
    mShaders["shadowPS"] = d3dUtil::CompileShader(L"Shadow.hlsl", nullptr, "PS", "ps_5_0");

    mShaders["debugVS"] = d3dUtil::CompileShader(L"ShadowDebug.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["debugPS"] = d3dUtil::CompileShader(L"ShadowDebug.hlsl", nullptr, "PS", "ps_5_0");
}

void InitDirect3DApp::BuildConstantBuffers()
{
    // ���� ������Ʈ ��� ����
    UINT size = sizeof(ObjectConstants);
    mObjectByteSize = ((size + 255) & ~255) * mRenderitems.size();
    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mObjectByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mObjectCB));

    mObjectCB->Map(0, nullptr, reinterpret_cast<void**>(&mObjectMappedData));


    // ���� ������Ʈ ���� ��� ����
    size = sizeof(MaterialConstants);
    mMaterialByteSize = ((size + 255) & ~255) * mMaterials.size();
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mMaterialByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mMaterialCB));

    mMaterialCB->Map(0, nullptr, reinterpret_cast<void**>(&mMaterialMappedData));


    // ���� ��� ����
    size = sizeof(PassConstants);
    mPassByteSize = ((size + 255) & ~255) * 2;
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mPassByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mPassCB));

    mPassCB->Map(0, nullptr, reinterpret_cast<void**>(&mPassMappedData));

    // Bone Transform ��� ����
    size = sizeof(SkinnedConstants);
    mSkinnedByteSize = ((size + 255) & ~255);
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mSkinnedByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mSkinnedCB));

    mSkinnedCB->Map(0, nullptr, reinterpret_cast<void**>(&mSkinnedMappedData));
}

void InitDirect3DApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE skyboxTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0), // t0 : Skybox Texture
    };

    CD3DX12_DESCRIPTOR_RANGE texTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1), // t1 : Object DIffuse Texture
    };

    CD3DX12_DESCRIPTOR_RANGE normalTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2), // t2 : Object Normal Texture
    };

    CD3DX12_DESCRIPTOR_RANGE shadowTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3), // t3 : Shadow Texture
    };

    CD3DX12_ROOT_PARAMETER param[8];
    param[0].InitAsConstantBufferView(0); // 0�� -> b0 -> CBV // ���� ������Ʈ ��� ����
    param[1].InitAsConstantBufferView(1); // 1�� -> b1 -> CBV // ���� ������Ʈ ���� ����
    param[2].InitAsConstantBufferView(2); // 2�� -> b2 -> CBV // ���� ��� ����
    param[3].InitAsDescriptorTable(_countof(skyboxTable), skyboxTable);
    param[4].InitAsDescriptorTable(_countof(texTable), texTable);
    param[5].InitAsDescriptorTable(_countof(normalTable), normalTable);
    param[6].InitAsDescriptorTable(_countof(shadowTable), shadowTable);
    param[7].InitAsConstantBufferView(3); // 3�� -> b3 -> CBV // Bone Transform ����

    auto staticSamplers = GetStaticSamplers();

    D3D12_ROOT_SIGNATURE_DESC sigDesc = CD3DX12_ROOT_SIGNATURE_DESC(_countof(param), param, (UINT)staticSamplers.size(), staticSamplers.data());
    sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // �Է� ������ �ܰ�

    ComPtr<ID3DBlob> blobSignature;
    ComPtr<ID3DBlob> blobError;
    ::D3D12SerializeRootSignature(&sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blobSignature, &blobError);
    md3dDevice->CreateRootSignature(0, blobSignature->GetBufferPointer(), blobSignature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
}

void InitDirect3DApp::BuildDescriptorHeaps()
{
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = (int)mTextures.size() + 1;     // ���� �ؽ�ó�� �׸��� �� �ؽ�ó ���� �߰�
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    std::vector<ComPtr<ID3D12Resource>> tex2DList =
    {
        mTextures["bricks"]->Resource,
        mTextures["bricksNormal"]->Resource,
        mTextures["stone"]->Resource,
        mTextures["tile"]->Resource,
        mTextures["tileNormal"]->Resource,
        mTextures["fence"]->Resource, 
        mTextures["default"]->Resource,
    };

    mSkinnedSrvHeapStart = (UINT)tex2DList.size();

    for (UINT i = 0; i < (UINT)mSkinnedTextureNames.size(); ++i)
    {
        auto texResource = mTextures[mSkinnedTextureNames[i]]->Resource;
        assert(texResource != nullptr);
        tex2DList.push_back(texResource);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (int i = 0; i < (int)tex2DList.size(); ++i)
    {
        srvDesc.Format = tex2DList[i]->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

        // next descriptor
        hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    }

    auto skyCubeMap = mTextures["skyCubeMap"]->Resource;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = skyCubeMap->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

    mSkyboxTexHeapIndex = (UINT)tex2DList.size();
    mShadowMapHeapIndex = mSkyboxTexHeapIndex + 1;

    auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

    mShadowMapSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize);

    // �׸��� ���� �ؽ�ó�� ����
    mShadowMap->BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));
}

void InitDirect3DApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    // 
    // PSO for skinned pass
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPsoDesc = opaquePsoDesc;
    skinnedPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
        mShaders["skinnedVS"]->GetBufferSize()
    };
    skinnedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

    //
    // PSO for alpha tested objects
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
    alphaTestedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
        mShaders["alphaTestedPS"]->GetBufferSize()
    };
    alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

    //
    // PSO for transparent objects
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

    //
    // PSO for Skybox
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Far < 1 �϶�, --> xyww w = 1
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyboxVS"]->GetBufferPointer()),
        mShaders["skyboxVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyboxPS"]->GetBufferPointer()),
        mShaders["skyboxPS"]->GetBufferSize()
    };

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["skybox"])));

    //
    // PSO for Shadow map pass
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = opaquePsoDesc;
    shadowPsoDesc.RasterizerState.DepthBias = 100000;
    shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    shadowPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
        mShaders["shadowVS"]->GetBufferSize()
    };
    shadowPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()),
        mShaders["shadowPS"]->GetBufferSize()
    };
    shadowPsoDesc.NumRenderTargets = 0;
    shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

    //
    // PSO for Shadow map pass
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedshadowPsoDesc = shadowPsoDesc;
    skinnedshadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedshadowPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedshadowVS"]->GetBufferPointer()),
        mShaders["skinnedshadowVS"]->GetBufferSize()
    };
    skinnedshadowPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()),
        mShaders["shadowPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedshadowPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow"])));

    //
    // PSO for Shadow map Debug pass
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
    debugPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
        mShaders["debugVS"]->GetBufferSize()
    };
    debugPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
        mShaders["debugPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> InitDirect3DApp::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWarp(
        0, // shader Register s0
        D3D12_FILTER_MIN_MAG_MIP_POINT,     // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,    // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        1, // shader Register s1
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,     // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, 
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        0.0f,
        16,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return {
        pointWarp,
        shadow
    };
}
