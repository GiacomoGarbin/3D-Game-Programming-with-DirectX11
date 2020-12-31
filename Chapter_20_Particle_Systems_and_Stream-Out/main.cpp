#include <D3DApp.h>

#include <cassert>
#include <string>
#include <sstream>

class TestApp : public D3DApp
{
public:
	TestApp();
	~TestApp();

	bool Init() override;
	void OnResize(GLFWwindow* window, int width, int height) override;
	void UpdateScene(float dt) override;
	void DrawScene() override;

	struct PerFrameCB
	{
		std::array<LightDirectional, 3> mLights;

		XMFLOAT3 mEyePositionW;
		float    pad1;

		float    mFogStart;
		float    mFogRange;
		XMFLOAT2 pad2;
		XMFLOAT4 mFogColor;

		float mHeightScale;
		float mMaxTessDistance;
		float mMinTessDistance;
		float mMinTessFactor;
		float mMaxTessFactor;
		XMFLOAT3 pad3;

		XMFLOAT4X4 mViewProj;

		float mTexelCellSpaceU;
		float mTexelCellSpaceV;
		float mWorldCellSpace;
		float pad4;
		XMFLOAT2 gTexelScale;
		XMFLOAT2 pad5;

		XMFLOAT4 gWorldFrustumPlanes[6];
	};

	static_assert((sizeof(PerFrameCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerFrameCB;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		Material mMaterial;
		XMFLOAT4X4 mTexCoordTransform;
		XMFLOAT4X4 mShadowTransform;
		XMFLOAT4X4 mWorldViewProjTexture;
	};

	static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerObjectCB;

	std::array<LightDirectional, 3> mLights;
	XMFLOAT3 mLightsCache[3];
	float mLightAngle;

	ID3D11SamplerState* mSamplerState;

	BoundingSphere mSceneBounds;

	ShadowMap mShadowMap;
	SSAO mSSAO;

	void DrawSceneToShadowMap();
	void DrawSceneToSSAONormalDepthMap();

	GameObject mSky;

	TerrainObject mTerrainObject;
	bool mWalkCameraMode;

	void ExtractFrustumPlanes(XMFLOAT4 planes[6], XMFLOAT4X4 M);
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr),
	mSamplerState(nullptr)
{
	mMainWindowTitle = "Ch20 Particle Systems and Stream-Out";

	//m4xMSAAEnabled = true;

	mLights[0].mAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	mLights[0].mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mLights[0].mSpecular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(0.707f, -0.707f, 0.0f);

	mLights[1].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[1].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	mLights[2].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(-0.57735f, -0.57735f, -0.57735f);

	for (UINT i = 0; i < 3; ++i)
	{
		mLightsCache[i] = mLights[i].mDirection;
	}

	mCamera.mPosition = XMFLOAT3(0, 2, 100);

	mWalkCameraMode = true;
}

TestApp::~TestApp()
{
	SafeRelease(mPerFrameCB);
	SafeRelease(mPerObjectCB);
	SafeRelease(mSamplerState);
}

bool TestApp::Init()
{
	if (!D3DApp::Init())
	{
		return false;
	}

	std::wstring base = L"";
	std::wstring proj = L"";

	// build per frame costant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerFrameCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mPerFrameCB));
	}

	// build per object constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerObjectCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mPerObjectCB));
	}

	// build sky geometry
	{
		GeometryGenerator::CreateSphere(5000, 3, mSky.mMesh);

		mSky.mRasterizerState = mNoCullRS;
		mSky.mDepthStencilState = mLessEqualDSS;

		mSky.mAlbedoSRV = mTextureManager.CreateSRV(L"grasscube1024.dds");

		// VS
		{
			std::wstring path = base + proj + L"SkyVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSky.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};
				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mSky.mInputLayout));
			}
		}

		// PS
		{
			std::wstring path = base + proj + L"SkyPS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSky.mPixelShader));
		}
	}

	// create vertex and input buffers
	{
		std::array<GameObject*, 1> objects =
		{
			&mSky
		};

		std::vector<GeometryGenerator::Vertex> vertices;
		std::vector<UINT> indices;

		auto AddVertex = [&vertices](GameObject& obj) -> void
		{
			obj.mVertexStart = vertices.size();
			vertices.insert(vertices.end(), obj.mMesh.mVertices.begin(), obj.mMesh.mVertices.end());
		};

		auto AddIndex = [&indices](GameObject& obj) -> void
		{
			obj.mIndexStart = indices.size();
			indices.insert(indices.end(), obj.mMesh.mIndices.begin(), obj.mMesh.mIndices.end());
		};

		for (GameObject* obj : objects)
		{
			AddVertex(*obj);
			AddIndex(*obj);
		}

		Microsoft::WRL::ComPtr<ID3D11Buffer> VB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> IB;

		// VB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * vertices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = vertices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &InitData, &VB));
		}

		// IB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * indices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = indices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &InitData, &IB));
		}

		for (GameObject* obj : objects)
		{
			obj->mVertexBuffer = VB;
			obj->mIndexBuffer = IB;
		}
	}

	// sampler state
	{
		D3D11_SAMPLER_DESC desc;
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
		desc.MinLOD = 0;
		desc.MaxLOD = 0;

		HR(mDevice->CreateSamplerState(&desc, &mSamplerState));
	}

	mShadowMap.Init(mDevice, 2048, 2048);
	mShadowMap.mDebugQuad.Init(mDevice, AspectRatio(), DebugQuad::ScreenCorner::BottomLeft, 1);

	mSSAO.Init(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mFarZ);
	mSSAO.mDebugQuad.Init(mDevice, AspectRatio(), DebugQuad::ScreenCorner::BottomRight, AspectRatio());

	// terrain object
	{
		TerrainObject::InitInfo info;
		info.HeightMapFileName = L"terrain.raw";
		info.LayerMapFileName.push_back(L"grass.dds");
		info.LayerMapFileName.push_back(L"darkdirt.dds");
		info.LayerMapFileName.push_back(L"stone.dds");
		info.LayerMapFileName.push_back(L"lightdirt.dds");
		info.LayerMapFileName.push_back(L"snow.dds");
		info.BlendMapFileName = L"blend.dds";
		info.HeightScale = 50.0f;
		info.HeightMapWidth = 2049;
		info.HeightMapDepth = 2049;
		info.CellSpacing = 0.5f;

		mTerrainObject.init(mDevice, mContext, mTextureManager, info);
	}

	// scene bounds
	{
		mSceneBounds.Center = XMFLOAT3(0, 0, 0);

		float HalfWidth = 0.5f * mTerrainObject.GetWidth();
		float HalfDepth = 0.5f * mTerrainObject.GetDepth();
		mSceneBounds.Radius = std::sqrt(HalfWidth * HalfWidth + HalfDepth * HalfDepth);
	}

	return true;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);

	mShadowMap.mDebugQuad.OnResize(AspectRatio());

	mSSAO.OnResize(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mFarZ);
	mSSAO.mDebugQuad.OnResize(AspectRatio());
}

void TestApp::UpdateScene(float dt)
{
	float step = 10;

	if (IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
	{
		step *= 10;
	}

	if (IsKeyPressed(GLFW_KEY_W))
	{
		mCamera.walk(+step * dt);
	}

	if (IsKeyPressed(GLFW_KEY_S))
	{
		mCamera.walk(-step * dt);
	}

	if (IsKeyPressed(GLFW_KEY_A))
	{
		mCamera.strafe(-step * dt);
	}

	if (IsKeyPressed(GLFW_KEY_D))
	{
		mCamera.strafe(+step * dt);
	}

	// animate lights
	{
		mLightAngle += 0.1f * dt;

		XMMATRIX R = XMMatrixRotationY(mLightAngle);

		for (UINT i = 0; i < 3; ++i)
		{
			XMVECTOR D = XMLoadFloat3(&mLightsCache[i]);
			D = XMVector3TransformNormal(D, R);
			XMStoreFloat3(&mLights[i].mDirection, D);
		}
	}

	// build shadow transform
	mShadowMap.BuildTranform(mLights[0].mDirection, mSceneBounds);

	if (IsKeyPressed(GLFW_KEY_Q))
	{
		mWalkCameraMode = false;
	}
	else
	{
		mWalkCameraMode = true;
	}

	// clamp camera to terrain surface in walk mode
	if (mWalkCameraMode)
	{
		XMFLOAT3& CameraPosition = mCamera.mPosition;

		float HalfWidth = 0.5f * mTerrainObject.GetWidth();
		float HalfDepth = 0.5f * mTerrainObject.GetDepth();

		CameraPosition.x = std::clamp(CameraPosition.x, -HalfWidth, +HalfWidth);
		CameraPosition.z = std::clamp(CameraPosition.z, -HalfDepth, +HalfDepth);

		float y = mTerrainObject.GetHeight(CameraPosition.x, CameraPosition.z);
		CameraPosition = XMFLOAT3(CameraPosition.x, y + 2.0f, CameraPosition.z);
	}

	mCamera.UpdateView();
}

void TestApp::DrawSceneToShadowMap()
{
	XMMATRIX view = XMLoadFloat4x4(&mShadowMap.mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mShadowMap.mLightProj);
	XMMATRIX ViewProj = view * proj;

	// draw terrain
	{
		// shaders
		{
			mContext->VSSetShader(mTerrainObject.mVertexShader, nullptr, 0);
			mContext->HSSetShader(mTerrainObject.mHullShader, nullptr, 0);
			mContext->DSSetShader(mTerrainObject.mDomainShader, nullptr, 0);
			mContext->PSSetShader(nullptr, nullptr, 0);
		}

		// input layout
		mContext->IASetInputLayout(mTerrainObject.mInputLayout);

		// primitive topology
		mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

		// vertex and index buffers
		{
			UINT stride = sizeof(TerrainObject::VertexData);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, &mTerrainObject.mPatchQuadVB, &stride, &offset);
			mContext->IASetIndexBuffer(mTerrainObject.mPatchQuadIB, DXGI_FORMAT_R32_UINT, 0);
		}

		// rasterizer, blend and depth-stencil states
		{
			mContext->RSSetState(mShadowMap.GetRS());

			FLOAT BlendFactor[] = { 0, 0, 0, 0 };
			mContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);

			mContext->OMSetDepthStencilState(nullptr, 0);
		}

		XMMATRIX W = mTerrainObject.mWorld;
		XMMATRIX WorldViewProj = W * ViewProj;
		XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);

		// transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T
		(
			+0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			+0.5f, +0.5f, 0.0f, 1.0f
		);

		// per frame constant buffer
		{
			PerFrameCB buffer;
			buffer.mLights[0] = mLights[0];
			buffer.mLights[1] = mLights[1];
			buffer.mLights[2] = mLights[2];
			buffer.mEyePositionW = mCamera.mPosition;
			buffer.mFogStart = 15;
			buffer.mFogRange = 175;
			XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
			buffer.mHeightScale = 0.07f;
			buffer.mMinTessDistance = 20;
			buffer.mMaxTessDistance = 500;
			buffer.mMinTessFactor = 0;
			buffer.mMaxTessFactor = 6;
			XMStoreFloat4x4(&buffer.mViewProj, ViewProj);

			buffer.mTexelCellSpaceU = 1.0f / mTerrainObject.mInitInfo.HeightMapWidth;
			buffer.mTexelCellSpaceV = 1.0f / mTerrainObject.mInitInfo.HeightMapDepth;
			buffer.mWorldCellSpace = mTerrainObject.mInitInfo.CellSpacing;

			buffer.gTexelScale = XMFLOAT2(50, 50);

			XMFLOAT4X4 M;
			XMStoreFloat4x4(&M, ViewProj);
			XMFLOAT4 WorldFrustumPlanes[6];
			ExtractFrustumPlanes(WorldFrustumPlanes, M);
			CopyMemory(buffer.gWorldFrustumPlanes, WorldFrustumPlanes, sizeof(buffer.gWorldFrustumPlanes));

			mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

			mContext->VSSetConstantBuffers(0, 1, &mPerFrameCB);
			mContext->HSSetConstantBuffers(0, 1, &mPerFrameCB);
			mContext->DSSetConstantBuffers(0, 1, &mPerFrameCB);
		}

		// per object constant buffer
		{
			PerObjectCB buffer;
			XMStoreFloat4x4(&buffer.mWorld, W);
			XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(W));
			XMStoreFloat4x4(&buffer.mWorldViewProj, WorldViewProj);
			buffer.mMaterial = mTerrainObject.mMaterial;
			XMStoreFloat4x4(&buffer.mTexCoordTransform, XMMatrixIdentity());
			XMStoreFloat4x4(&buffer.mShadowTransform, W * S);
			XMStoreFloat4x4(&buffer.mWorldViewProjTexture, WorldViewProj * T);

			mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

			mContext->VSSetConstantBuffers(1, 1, &mPerObjectCB);
		}

		// SRVs
		{
			mContext->VSSetShaderResources(0, 1, &mTerrainObject.mHeightMapSRV);
			mContext->DSSetShaderResources(0, 1, &mTerrainObject.mHeightMapSRV);
		}

		// sampler states
		{
			mContext->VSSetSamplers(0, 1, &mTerrainObject.mHeightMapSS);
			mContext->DSSetSamplers(0, 1, &mTerrainObject.mHeightMapSS);
		}

		// draw call
		{
			mContext->DrawIndexed(mTerrainObject.mPatchQuadFaces * 4, 0, 0);
		}

		// unbind SRVs
		{
			ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
			mContext->VSSetShaderResources(0, 1, NullSRV);
			mContext->DSSetShaderResources(0, 1, NullSRV);
		}

		// unbind shaders
		{
			mContext->HSSetShader(nullptr, nullptr, 0);
			mContext->DSSetShader(nullptr, nullptr, 0);
		}
	}
}

void TestApp::DrawSceneToSSAONormalDepthMap()
{
	//auto SetPerObjectCB = [this](GameObject* obj) -> void
	//{
	//	XMMATRIX view = XMLoadFloat4x4(&mCamera.mView);
	//	XMMATRIX WorldView = obj->mWorld * view;

	//	SSAO::NormalDepthCB buffer;
	//	XMStoreFloat4x4(&buffer.WorldView, WorldView);
	//	XMStoreFloat4x4(&buffer.WorldViewProj, WorldView * mCamera.mProj);
	//	XMStoreFloat4x4(&buffer.WorldInverseTransposeView, GameMath::InverseTranspose(obj->mWorld) * view);
	//	XMStoreFloat4x4(&buffer.TexCoordTransform, obj->mTexCoordTransform);
	//	mContext->UpdateSubresource(mSSAO.GetNormalDepthCB(), 0, nullptr, &buffer, 0, 0);
	//	mContext->VSSetConstantBuffers(0, 1, &mSSAO.GetNormalDepthCB());
	//};

	//auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	//{
	//	FLOAT BlendFactor[] = { 0, 0, 0, 0 };

	//	// shaders
	//	mContext->VSSetShader(mSSAO.GetNormalDepthVS(), nullptr, 0);
	//	mContext->PSSetShader(mSSAO.GetNormalDepthPS(), nullptr, 0);

	//	// input layout
	//	mContext->IASetInputLayout(mSSAO.GetNormalDepthIL());

	//	// primitive topology
	//	mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

	//	// vertex and index buffers
	//	if (obj->mInstancedBuffer)
	//	{
	//		UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
	//		UINT offset[2] = { 0, 0 };

	//		ID3D11Buffer* vbs[2] = { obj->mVertexBuffer.Get(), obj->mInstancedBuffer };

	//		mContext->IASetVertexBuffers(0, 2, vbs, stride, offset);
	//		mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	//	}
	//	else
	//	{
	//		UINT stride = sizeof(GeometryGenerator::Vertex);
	//		UINT offset = 0;

	//		mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
	//		mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	//	}

	//	//  per object constant buffer
	//	SetPerObjectCB(obj);

	//	// textures
	//	{
	//		mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);

	//		// TODO : add normal mapping ?

	//		//mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
	//		//mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
	//	}

	//	// rasterizer, blend and depth-stencil states
	//	mContext->RSSetState(obj->mRasterizerState.Get());
	//	mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
	//	mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

	//	// draw call
	//	if (obj->mIndexBuffer && obj->mInstancedBuffer)
	//	{
	//		mContext->DrawIndexedInstanced(obj->mMesh.mIndices.size(), obj->mVisibleInstanceCount, 0, 0, 0);
	//	}
	//	else if (obj->mIndexBuffer)
	//	{
	//		mContext->DrawIndexed(obj->mMesh.mIndices.size(), obj->mIndexStart, obj->mVertexStart);
	//	}
	//	else
	//	{
	//		mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
	//	}

	//	// unbind SRV
	//	ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
	//	mContext->PSSetShaderResources(0, 1, NullSRV);
	//};

	//DrawGameObject(&mGrid);
	//DrawGameObject(&mBox);
	//DrawGameObject(&mSkull);

	//for (UINT i = 0; i < 5; ++i)
	//{
	//	mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
	//	DrawGameObject(&mCylinder);
	//	mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
	//	DrawGameObject(&mCylinder);

	//	mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
	//	DrawGameObject(&mSphere);
	//	mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
	//	DrawGameObject(&mSphere);
	//}

	//for (GameObjectInstance* instance : mObjectInstances)
	//{
	//	GameObject* obj = instance->obj;

	//	// vertex shader
	//	{
	//		mContext->VSSetShader(mSSAO.GetNormalDepthVS(obj->mIsSkinned), nullptr, 0);
	//	}

	//	// input layout
	//	mContext->IASetInputLayout(mSSAO.GetNormalDepthIL(obj->mIsSkinned));

	//	// primitive topology
	//	mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

	//	// vertex and index buffers
	//	{
	//		UINT stride = sizeof(GeometryGenerator::Vertex);
	//		UINT offset = 0;

	//		mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
	//		mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	//	}

	//	// rasterizer, blend and depth-stencil states
	//	{
	//		mContext->RSSetState(obj->mRasterizerState.Get());

	//		FLOAT BlendFactor[] = { 0, 0, 0, 0 };
	//		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

	//		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
	//	}

	//	// per object constant buffer
	//	{
	//		XMMATRIX view = XMLoadFloat4x4(&mCamera.mView);
	//		XMMATRIX WorldView = instance->world * view;

	//		SSAO::NormalDepthCB buffer;
	//		XMStoreFloat4x4(&buffer.WorldView, WorldView);
	//		XMStoreFloat4x4(&buffer.WorldViewProj, WorldView * mCamera.mProj);
	//		XMStoreFloat4x4(&buffer.WorldInverseTransposeView, GameMath::InverseTranspose(instance->world) * view);
	//		XMStoreFloat4x4(&buffer.TexCoordTransform, obj->mTexCoordTransform);
	//		mContext->UpdateSubresource(mSSAO.GetNormalDepthCB(), 0, nullptr, &buffer, 0, 0);
	//		mContext->VSSetConstantBuffers(0, 1, &mSSAO.GetNormalDepthCB());
	//	}

	//	// per skinned constant buffer
	//	{
	//		PerSkinnedCB buffer;
	//		ZeroMemory(&buffer.mBoneTransforms, sizeof(buffer.mBoneTransforms));
	//		CopyMemory(&buffer.mBoneTransforms, instance->transforms.data(), instance->transforms.size() * sizeof(XMFLOAT4X4));

	//		mContext->UpdateSubresource(mPerSkinnedCB, 0, 0, &buffer, 0, 0);
	//		mContext->VSSetConstantBuffers(2, 1, &mPerSkinnedCB);
	//	}

	//	for (UINT i = 0; i < obj->mSubsets.size(); ++i)
	//	{
	//		// pixel shader
	//		{
	//			mContext->PSSetShader(mSSAO.GetNormalDepthPS(obj->mIsAlphaClipping[i]), nullptr, 0);
	//		}

	//		// bind SRVs
	//		{
	//			mContext->PSSetShaderResources(0, 1, &obj->mDiffuseMapSRVs[i]);
	//			//mContext->PSSetShaderResources(1, 1, &obj->mNormalMapSRVs[i]);
	//		}

	//		// draw call
	//		const Subset& subset = obj->mSubsets[i];
	//		mContext->DrawIndexed(subset.FaceCount * 3, obj->mIndexStart + subset.FaceStart * 3, obj->mVertexStart);

	//		// unbind SRVs
	//		{
	//			ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
	//			mContext->PSSetShaderResources(0, 2, NullSRVs);
	//		}
	//	}
	//}
}

void TestApp::ExtractFrustumPlanes(XMFLOAT4 planes[6], XMFLOAT4X4 M)
{
	// left
	planes[0].x = M(0, 3) + M(0, 0);
	planes[0].y = M(1, 3) + M(1, 0);
	planes[0].z = M(2, 3) + M(2, 0);
	planes[0].w = M(3, 3) + M(3, 0);

	// right
	planes[1].x = M(0, 3) - M(0, 0);
	planes[1].y = M(1, 3) - M(1, 0);
	planes[1].z = M(2, 3) - M(2, 0);
	planes[1].w = M(3, 3) - M(3, 0);

	// bottom
	planes[2].x = M(0, 3) + M(0, 1);
	planes[2].y = M(1, 3) + M(1, 1);
	planes[2].z = M(2, 3) + M(2, 1);
	planes[2].w = M(3, 3) + M(3, 1);

	// top
	planes[3].x = M(0, 3) - M(0, 1);
	planes[3].y = M(1, 3) - M(1, 1);
	planes[3].z = M(2, 3) - M(2, 1);
	planes[3].w = M(3, 3) - M(3, 1);

	// near
	planes[4].x = M(0, 2);
	planes[4].y = M(1, 2);
	planes[4].z = M(2, 2);
	planes[4].w = M(3, 2);

	// far
	planes[5].x = M(0, 3) - M(0, 2);
	planes[5].y = M(1, 3) - M(1, 2);
	planes[5].z = M(2, 3) - M(2, 2);
	planes[5].w = M(3, 3) - M(3, 2);

	// normalize the plane equations
	for (int i = 0; i < 6; ++i)
	{
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
		XMStoreFloat4(&planes[i], v);
	}
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	// bind shadow map dsv and set null render target
	mShadowMap.BindDSVAndSetNullRenderTarget(mContext);
	// draw scene to shadow map
	DrawSceneToShadowMap();

	// restore rasterazer state -> no need for this, DrawGameObject sets the rasterazer state for each object

	// restore viewport
	mContext->RSSetViewports(1, &mViewport);

	// render view space normals and depths
	mSSAO.BindNormalDepthRenderTarget(mContext, mDepthStencilView);
	DrawSceneToSSAONormalDepthMap();

	if (!IsKeyPressed(GLFW_KEY_4))
	{
		// compute ambient occlusion
		mSSAO.ComputeAmbientMap(mContext, mCamera);
		// blur ambient map
		mSSAO.BlurAmbientMap(mContext, 4);
	}
	else
	{
		mContext->OMSetRenderTargets(1, &mSSAO.GetAmbientMapRTV(), nullptr);
		mContext->ClearRenderTargetView(mSSAO.GetAmbientMapRTV(), Colors::White);
	}

	// restore back and depth buffers, and viewport
	mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	mContext->RSSetViewports(1, &mViewport);

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	// bind shadow map and ambient map as SRV
	mContext->PSSetShaderResources(3, 1, &mShadowMap.GetSRV());
	mContext->PSSetShaderResources(4, 1, &mSSAO.GetAmbientMapSRV());

	auto SetPerFrameCB = [this]() -> void
	{
		PerFrameCB buffer;
		buffer.mLights[0] = mLights[0];
		buffer.mLights[1] = mLights[1];
		buffer.mLights[2] = mLights[2];
		buffer.mEyePositionW = mCamera.mPosition;
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
		buffer.mHeightScale = 0.07f;
		buffer.mMaxTessDistance = 1;
		buffer.mMinTessDistance = 25;
		buffer.mMinTessFactor = 1;
		buffer.mMaxTessFactor = 5;
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mViewProj, V * mCamera.mProj);

		mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);

		// transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T
		(
			+0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			+0.5f, +0.5f, 0.0f, 1.0f
		);

		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * mCamera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexCoordTransform, obj->mTexCoordTransform);
		XMStoreFloat4x4(&buffer.mShadowTransform, obj->mWorld * S);
		XMStoreFloat4x4(&buffer.mWorldViewProjTexture, obj->mWorld * V * mCamera.mProj * T);

		mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
		//mContext->HSSetShader(obj->mHullShader.Get(), nullptr, 0);
		//mContext->DSSetShader(obj->mDomainShader.Get(), nullptr, 0);
		//mContext->GSSetShader(obj->mGeometryShader.Get(), nullptr, 0);
		mContext->PSSetShader(obj->mPixelShader.Get(), nullptr, 0);

		// input layout
		mContext->IASetInputLayout(obj->mInputLayout.Get());

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		if (obj->mInstancedBuffer)
		{
			UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
			UINT offset[2] = { 0, 0 };

			ID3D11Buffer* vbs[2] = { obj->mVertexBuffer.Get(), obj->mInstancedBuffer };

			mContext->IASetVertexBuffers(0, 2, vbs, stride, offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}
		else
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		// constant buffer per object
		SetPerObjectCB(obj);

		// bind SRVs
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);
			mContext->DSSetShaderResources(1, 1, &obj->mNormalSRV);
			mContext->PSSetShaderResources(1, 1, &obj->mNormalSRV);
		}

		// bind sampler states
		{
			mContext->DSSetSamplers(0, 1, &mSamplerState);
			mContext->PSSetSamplers(0, 1, &mSamplerState);

			mContext->PSSetSamplers(1, 1, &mShadowMap.GetSS());
		}

		// rasterizer, blend and depth-stencil states

		if (IsKeyPressed(GLFW_KEY_1))
		{
			mContext->RSSetState(mWireframeRS.Get());
		}
		else
		{
			mContext->RSSetState(obj->mRasterizerState.Get());
		}

		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

		if (obj->mDepthStencilState.Get() != nullptr || IsKeyPressed(GLFW_KEY_1))
		{
			mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
		}
		else
		{
			mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);
		}

		// draw call
		if (obj->mIndexBuffer && obj->mInstancedBuffer)
		{
			mContext->DrawIndexedInstanced(obj->mMesh.mIndices.size(), obj->mVisibleInstanceCount, 0, 0, 0);
		}
		else if (obj->mIndexBuffer)
		{
			mContext->DrawIndexed(obj->mMesh.mIndices.size(), obj->mIndexStart, obj->mVertexStart);
		}
		else
		{
			mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
		}

		// unbind SRVs
		ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
		mContext->PSSetShaderResources(0, 2, NullSRV);
	};

	// draw terrain
	{
		// shaders
		{
			mContext->VSSetShader(mTerrainObject.mVertexShader, nullptr, 0);
			mContext->HSSetShader(mTerrainObject.mHullShader, nullptr, 0);
			mContext->DSSetShader(mTerrainObject.mDomainShader, nullptr, 0);
			mContext->PSSetShader(mTerrainObject.mPixelShader, nullptr, 0);
		}

		// input layout
		mContext->IASetInputLayout(mTerrainObject.mInputLayout);

		// primitive topology
		mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

		// vertex and index buffers
		{
			UINT stride = sizeof(TerrainObject::VertexData);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, &mTerrainObject.mPatchQuadVB, &stride, &offset);
			mContext->IASetIndexBuffer(mTerrainObject.mPatchQuadIB, DXGI_FORMAT_R32_UINT, 0);
		}

		// rasterizer, blend and depth-stencil states
		{
			if (IsKeyPressed(GLFW_KEY_1))
			{
				mContext->RSSetState(mWireframeRS.Get());
			}
			else
			{
				mContext->RSSetState(nullptr);
			}

			FLOAT BlendFactor[] = { 0, 0, 0, 0 };
			mContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);

			mContext->OMSetDepthStencilState(nullptr, 0);
			//mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);
		}

		XMMATRIX W = mTerrainObject.mWorld;
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX ViewProj = V * mCamera.mProj;
		XMMATRIX WorldViewProj = W * ViewProj;
		XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);

		// transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T
		(
			+0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			+0.5f, +0.5f, 0.0f, 1.0f
		);

		// per frame constant buffer
		{
			PerFrameCB buffer;
			buffer.mLights[0] = mLights[0];
			buffer.mLights[1] = mLights[1];
			buffer.mLights[2] = mLights[2];
			buffer.mEyePositionW = mCamera.mPosition;
			buffer.mFogStart = 15;
			buffer.mFogRange = 175;
			XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
			buffer.mHeightScale = 0.07f;
			buffer.mMinTessDistance = 20;
			buffer.mMaxTessDistance = 500;
			buffer.mMinTessFactor = 0;
			buffer.mMaxTessFactor = 6;
			XMStoreFloat4x4(&buffer.mViewProj, ViewProj);

			buffer.mTexelCellSpaceU = 1.0f / mTerrainObject.mInitInfo.HeightMapWidth;
			buffer.mTexelCellSpaceV = 1.0f / mTerrainObject.mInitInfo.HeightMapDepth;
			buffer.mWorldCellSpace = mTerrainObject.mInitInfo.CellSpacing;

			buffer.gTexelScale = XMFLOAT2(50, 50);

			XMFLOAT4X4 M;
			XMStoreFloat4x4(&M, ViewProj);
			XMFLOAT4 WorldFrustumPlanes[6];
			ExtractFrustumPlanes(WorldFrustumPlanes, M);
			CopyMemory(buffer.gWorldFrustumPlanes, WorldFrustumPlanes, sizeof(buffer.gWorldFrustumPlanes));

			mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

			mContext->VSSetConstantBuffers(0, 1, &mPerFrameCB);
			mContext->HSSetConstantBuffers(0, 1, &mPerFrameCB);
			mContext->DSSetConstantBuffers(0, 1, &mPerFrameCB);
			mContext->PSSetConstantBuffers(0, 1, &mPerFrameCB);
		}

		// per object constant buffer
		{
			PerObjectCB buffer;
			XMStoreFloat4x4(&buffer.mWorld, W);
			XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(W));
			XMStoreFloat4x4(&buffer.mWorldViewProj, WorldViewProj);
			buffer.mMaterial = mTerrainObject.mMaterial;
			XMStoreFloat4x4(&buffer.mTexCoordTransform, XMMatrixIdentity());
			XMStoreFloat4x4(&buffer.mShadowTransform, W * S);
			XMStoreFloat4x4(&buffer.mWorldViewProjTexture, WorldViewProj * T);

			mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

			mContext->VSSetConstantBuffers(1, 1, &mPerObjectCB);
			mContext->DSSetConstantBuffers(1, 1, &mPerObjectCB);
			mContext->PSSetConstantBuffers(1, 1, &mPerObjectCB);
		}

		// SRVs
		{
			mContext->VSSetShaderResources(0, 1, &mTerrainObject.mHeightMapSRV);
			mContext->DSSetShaderResources(0, 1, &mTerrainObject.mHeightMapSRV);

			ID3D11ShaderResourceView* const SRVs[4] =
			{
				mTerrainObject.mHeightMapSRV,
				mTerrainObject.mLayerMapArraySRV,
				mTerrainObject.mBlendMapSRV,
				mShadowMap.GetSRV()
			};
			mContext->PSSetShaderResources(0, 4, SRVs);
		}

		// sampler states
		{
			mContext->VSSetSamplers(0, 1, &mTerrainObject.mHeightMapSS);
			mContext->DSSetSamplers(0, 1, &mTerrainObject.mHeightMapSS);
			mContext->PSSetSamplers(0, 1, &mTerrainObject.mHeightMapSS);

			mContext->PSSetSamplers(1, 1, &mSamplerState);
			mContext->PSSetSamplers(2, 1, &mShadowMap.GetSS());
		}

		// draw call
		{
			mContext->DrawIndexed(mTerrainObject.mPatchQuadFaces * 4, 0, 0);
		}

		// unbind SRVs
		{
			ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
			mContext->VSSetShaderResources(0, 1, NullSRV);
			mContext->DSSetShaderResources(0, 1, NullSRV);

			ID3D11ShaderResourceView* const NullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
			mContext->PSSetShaderResources(0, 4, NullSRVs);
		}

		// unbind shaders
		{
			mContext->HSSetShader(nullptr, nullptr, 0);
			mContext->DSSetShader(nullptr, nullptr, 0);
		}
	}

	SetPerFrameCB();

	// draw sky
	DrawGameObject(&mSky);

	if (IsKeyPressed(GLFW_KEY_2))
	{
		mShadowMap.mDebugQuad.Draw(mContext, mShadowMap.GetSRV());
	}

	if (IsKeyPressed(GLFW_KEY_3))
	{
		mSSAO.mDebugQuad.Draw(mContext, mSSAO.GetAmbientMapSRV());
		//mSSAO.mDebugQuad.Draw(mContext, mSSAO.GetNormalDepthSRV());
	}

	// reset depth stencil state
	//mContext->OMSetDepthStencilState(nullptr, 0);

	// unbind shadow map and ambient map as SRV
	ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
	mContext->PSSetShaderResources(3, 2, NullSRV);

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}