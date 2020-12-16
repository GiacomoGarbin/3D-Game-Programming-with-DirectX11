#include <D3DApp.h>

#include <cassert>
#include <string>
#include <array>
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
	};

	static_assert((sizeof(PerFrameCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerFrameCB;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		GameObject::Material mMaterial;
		XMFLOAT4X4 mTexCoordTransform;
		XMFLOAT4X4 mShadowTransform;
		XMFLOAT4X4 mWorldViewProjTexture;
	};

	static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerObjectCB;

	GameObject mSkull;
	GameObject mBox;
	GameObject mGrid;
	GameObject mSphere;
	GameObject mCylinder;
	GameObject mSky;

	std::array<LightDirectional, 3> mLights;
	XMFLOAT3 mLightsCache[3];
	float mLightAngle;

	ID3D11SamplerState* mSamplerState;

	BoundingSphere mSceneBounds;

	ShadowMap mShadowMap;
	SSAO mSSAO;

	void DrawSceneToShadowMap();
	void DrawSceneToSSAONormalDepthMap();
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr),
	mSamplerState(nullptr)
{
	mMainWindowTitle = "Ch22 Ambient Occlusion";

	//m4xMSAAEnabled = true;

	mLights[0].mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[0].mDiffuse = XMFLOAT4(0.7f, 0.7f, 0.6f, 1.0f);
	mLights[0].mSpecular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(-0.57735f, -0.57735f, -0.57735f);

	mLights[1].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse = XMFLOAT4(0.40f, 0.40f, 0.40f, 1.0f);
	mLights[1].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(-0.707f, 0.707f, 0.0f);

	mLights[2].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(0.0f, 0.0, -1.0f);

	for (UINT i = 0; i < 3; ++i) mLightsCache[i] = mLights[i].mDirection;

	mCamera.mPosition = XMFLOAT3(0, 2, -15);

	mSceneBounds.Center = XMFLOAT3(0, 0, 0);
	mSceneBounds.Radius = std::sqrt(10 * 10 + 15 * 15);
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

	//std::wstring base = L"C:/Users/ggarbin/Desktop/3D-Game-Programming-with-DirectX11/";
	//std::wstring proj = L"Chapter 21 Shadow Mapping/";

	std::wstring base = L"";
	std::wstring proj = L"";

	// VS
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
		std::wstring path = base + proj + L"VS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mSkull.mVertexShader = shader;
		mGrid.mVertexShader = shader;
		mBox.mVertexShader = shader;
		mCylinder.mVertexShader = shader;
		mSphere.mVertexShader = shader;

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

			mSkull.mInputLayout = layout;
			mGrid.mInputLayout = layout;
			mBox.mInputLayout = layout;
			mCylinder.mInputLayout = layout;
			mSphere.mInputLayout = layout;
		}
	}

	// HS
	{
		Microsoft::WRL::ComPtr<ID3D11HullShader> shader;
		std::wstring path = base + proj + L"HS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "hs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateHullShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mHullShader = shader;
		mBox.mHullShader = shader;
		mCylinder.mHullShader = shader;
	}

	// DS
	{
		Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
		std::wstring path = base + proj + L"DS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ds_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateDomainShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mDomainShader = shader;
		mBox.mDomainShader = shader;
		mCylinder.mDomainShader = shader;
	}

	// PS
	{
		Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
		std::wstring path = base + proj + L"PS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_TEXTURE",         "1" });
		defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
		defines.push_back({ "ENABLE_NORMAL_MAPPING",  "1" });
		//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
		//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		defines.push_back({ "ENABLE_REFLECTION",      "0" });
		defines.push_back({ "ENABLE_FOG",             "0" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mPixelShader = shader;
		mBox.mPixelShader = shader;
		mCylinder.mPixelShader = shader;
	}

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

	// build skull geometry
	{
		GeometryGenerator::CreateSkull(mSkull.mMesh);

		mSkull.mVertexStart = 0;
		mSkull.mIndexStart = 0;

		mSkull.mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f);

		mSkull.mMaterial.mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
		mSkull.mMaterial.mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
		mSkull.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mSkull.mMaterial.mReflect = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
			defines.push_back({ "ENABLE_NORMAL_MAPPING",  "0" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSkull.mPixelShader));
		}
	}

	// build grid geometry
	{
		GeometryGenerator::CreateGrid(20, 30, 50, 40, mGrid.mMesh);

		mGrid.mVertexStart = mSkull.mVertexStart + mSkull.mMesh.mVertices.size();
		mGrid.mIndexStart = mSkull.mIndexStart + mSkull.mMesh.mIndices.size();

		mGrid.mWorld = XMMatrixIdentity();
		mGrid.mTexCoordTransform = XMMatrixScaling(8, 10, 1);

		mGrid.mMaterial.mAmbient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mGrid.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mGrid.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);
		mGrid.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		//mGrid.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		CreateSRV(L"stones.dds", &mGrid.mAlbedoSRV);
		CreateSRV(L"stones_n.dds", &mGrid.mNormalSRV);
	}

	// build box geometry
	{
		GeometryGenerator::CreateBox(1, 1, 1, mBox.mMesh);

		mBox.mVertexStart = mGrid.mVertexStart + mGrid.mMesh.mVertices.size();
		mBox.mIndexStart = mGrid.mIndexStart + mGrid.mMesh.mIndices.size();

		mBox.mWorld = XMMatrixScaling(3.0f, 1.0f, 3.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
		mBox.mTexCoordTransform = XMMatrixScaling(2, 1, 1);

		mBox.mMaterial.mAmbient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mBox.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		//mBox.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		CreateSRV(L"floor.dds", &mBox.mAlbedoSRV);
		CreateSRV(L"floor_n.dds", &mBox.mNormalSRV);
	}

	// build cylinder geometry
	{
		GeometryGenerator::CreateCylinder(0.5f, 0.5f, 3, 15, 15, mCylinder.mMesh);

		mCylinder.mVertexStart = mBox.mVertexStart + mBox.mMesh.mVertices.size();
		mCylinder.mIndexStart = mBox.mIndexStart + mBox.mMesh.mIndices.size();

		mCylinder.mTexCoordTransform = XMMatrixScaling(1, 2, 1);

		mCylinder.mMaterial.mAmbient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mCylinder.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mCylinder.mMaterial.mSpecular = XMFLOAT4(1.0f, 1.0f, 1.0f, 32.0f);
		mCylinder.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		//mCylinder.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		CreateSRV(L"bricks.dds", &mCylinder.mAlbedoSRV);
		CreateSRV(L"bricks_n.dds", &mCylinder.mNormalSRV);
	}

	// build sphere geometry
	{
		GeometryGenerator::CreateSphere(0.5f, 3, mSphere.mMesh);

		mSphere.mVertexStart = mCylinder.mVertexStart + mCylinder.mMesh.mVertices.size();
		mSphere.mIndexStart = mCylinder.mIndexStart + mCylinder.mMesh.mIndices.size();

		mSphere.mMaterial.mAmbient = XMFLOAT4(0.2f, 0.3f, 0.4f, 1.0f);
		mSphere.mMaterial.mDiffuse = XMFLOAT4(0.2f, 0.3f, 0.4f, 1.0f);
		mSphere.mMaterial.mSpecular = XMFLOAT4(0.9f, 0.9f, 0.9f, 16.0f);
		mSphere.mMaterial.mReflect = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "1" });
			defines.push_back({ "ENABLE_NORMAL_MAPPING",  "0" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSphere.mPixelShader));
		}
	}

	// build sky geometry
	{
		GeometryGenerator::CreateSphere(5000, 3, mSky.mMesh);

		mSky.mVertexStart = mSphere.mVertexStart + mSphere.mMesh.mVertices.size();
		mSky.mIndexStart = mSphere.mIndexStart + mSphere.mMesh.mIndices.size();

		mSky.mRasterizerState = mNoCullRS;
		mSky.mDepthStencilState = mLessEqualDSS;

		CreateSRV(L"desertcube1024.dds", &mSky.mAlbedoSRV);

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
		std::array<GameObject*, 6> objects =
		{
			&mSkull,
			&mGrid,
			&mBox,
			&mCylinder,
			&mSphere,
			&mSky
		};

		std::vector<GeometryGenerator::Vertex> vertices;
		std::vector<UINT> indices;

		auto AddVertex = [&vertices](const GameObject& obj) -> void
		{
			vertices.insert(vertices.end(), obj.mMesh.mVertices.begin(), obj.mMesh.mVertices.end());
		};

		auto AddIndex = [&indices](const GameObject& obj) -> void
		{
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

		mContext->DSSetSamplers(0, 1, &mSamplerState);
		mContext->PSSetSamplers(0, 1, &mSamplerState);
	}

	mShadowMap.Init(mDevice, 2048, 2048);
	mShadowMap.mDebugQuad.Init(mDevice, AspectRatio(), DebugQuad::ScreenCorner::BottomLeft, 1);
	//mContext->PSSetShaderResources(3, 1, &mShadowMap.GetSRV());
	mContext->PSSetSamplers(1, 1, &mShadowMap.GetSS());

	mSSAO.Init(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mFarZ);
	mSSAO.mDebugQuad.Init(mDevice, AspectRatio(), DebugQuad::ScreenCorner::BottomRight, AspectRatio());

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
	if (IsKeyPressed(GLFW_KEY_W))
	{
		mCamera.walk(+10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_S))
	{
		mCamera.walk(-10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_A))
	{
		mCamera.strafe(-10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_D))
	{
		mCamera.strafe(+10 * dt);
	}

	mLightAngle += 0.1f * dt;

	XMMATRIX R = XMMatrixRotationY(mLightAngle);

	for (UINT i = 0; i < 3; ++i)
	{
		XMVECTOR D = XMLoadFloat3(&mLightsCache[i]);
		D = XMVector3TransformNormal(D, R);
		XMStoreFloat3(&mLights[i].mDirection, D);
	}

	// build shadow transform
	mShadowMap.BuildTranform(mLights[0].mDirection, mSceneBounds);

	mCamera.UpdateView();
}

void TestApp::DrawSceneToShadowMap()
{
	XMMATRIX view = XMLoadFloat4x4(&mShadowMap.mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mShadowMap.mLightProj);
	XMMATRIX ViewProj = XMMatrixMultiply(view, proj);

	auto SetPerObjectCB = [this, &ViewProj](GameObject* obj) -> void
	{
		ShadowMap::PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * ViewProj);
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexCoordTransform);
		mContext->UpdateSubresource(mShadowMap.GetCB(), 0, nullptr, &buffer, 0, 0);
		mContext->VSSetConstantBuffers(0, 1, &mShadowMap.GetCB());
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(mShadowMap.GetVS(), nullptr, 0);
		mContext->PSSetShader(nullptr, nullptr, 0);

		// input layout
		mContext->IASetInputLayout(mShadowMap.GetIL());

		// primitive topology
		mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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

		//  per object constant buffer
		SetPerObjectCB(obj);

		// textures
		{
			mContext->PSSetShaderResources(0, 1, obj->mAlbedoSRV.GetAddressOf());
			//mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
			//mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
		}

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(mShadowMap.GetRS());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

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

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
		mContext->PSSetShaderResources(0, 2, NullSRV);
	};

	// draw without reflection
	{
		DrawGameObject(&mGrid);
		DrawGameObject(&mBox);

		for (UINT i = 0; i < 5; ++i)
		{
			mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder);
			mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder);
		}
	}

	// draw with reflection
	{
		// bind cube map SRV
		mContext->PSSetShaderResources(2, 1, mSky.mAlbedoSRV.GetAddressOf());

		DrawGameObject(&mSkull);

		for (UINT i = 0; i < 5; ++i)
		{
			mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere);
			mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere);
		}

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(2, 1, &NullSRV);
	}
}

void TestApp::DrawSceneToSSAONormalDepthMap()
{
	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		XMMATRIX view = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX WorldView = obj->mWorld * view;

		SSAO::NormalDepthCB buffer;
		XMStoreFloat4x4(&buffer.WorldView, WorldView);
		XMStoreFloat4x4(&buffer.WorldViewProj, WorldView * mCamera.mProj);
		XMStoreFloat4x4(&buffer.WorldInverseTransposeView, GameMath::InverseTranspose(obj->mWorld) * view);
		XMStoreFloat4x4(&buffer.TexCoordTransform, obj->mTexCoordTransform);
		mContext->UpdateSubresource(mSSAO.GetNormalDepthCB(), 0, nullptr, &buffer, 0, 0);
		mContext->VSSetConstantBuffers(0, 1, &mSSAO.GetNormalDepthCB());
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(mSSAO.GetNormalDepthVS(), nullptr, 0);
		mContext->PSSetShader(mSSAO.GetNormalDepthPS(), nullptr, 0);

		// input layout
		mContext->IASetInputLayout(mSSAO.GetNormalDepthIL());

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

		//  per object constant buffer
		SetPerObjectCB(obj);

		// textures
		{
			mContext->PSSetShaderResources(0, 1, obj->mAlbedoSRV.GetAddressOf());

			// TODO : add normal mapping ?

			//mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
			//mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
		}

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(obj->mRasterizerState.Get());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

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

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
		mContext->PSSetShaderResources(0, 1, NullSRV);
	};

	DrawGameObject(&mGrid);
	DrawGameObject(&mBox);

	for (UINT i = 0; i < 5; ++i)
	{
		mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);
		mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);

		mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
		mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
	}

	DrawGameObject(&mSkull);
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

	// compute ambient occlusion
	mSSAO.ComputeAmbientMap(mContext, mCamera);
	// blur ambient map
	mSSAO.BlurAmbientMap(mContext, 4);

	// restore back and depth buffers, and viewport
	mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	mContext->RSSetViewports(1, &mViewport);

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	//mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);

	// bind shadow map and ambient map as SRV
	mContext->PSSetShaderResources(3, 1, &mShadowMap.GetSRV());
	mContext->PSSetShaderResources(4, 1, &mSSAO.GetAmbientMapSRV());
	//mContext->PSSetShaderResources(4, 1, mSSAO.GetAmbientMapSRV().GetAddressOf());

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
		mContext->DSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);

		// transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T
		(
			+0.5f,  0.0f, 0.0f, 0.0f,
			 0.0f, -0.5f, 0.0f, 0.0f,
			 0.0f,  0.0f, 1.0f, 0.0f,
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
			mContext->PSSetShaderResources(0, 1, obj->mAlbedoSRV.GetAddressOf());
			mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
			mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
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

		if (obj->mDepthStencilState.Get() != nullptr)
		{
			mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
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

	SetPerFrameCB();

	// draw without reflection
	{
		DrawGameObject(&mGrid);
		DrawGameObject(&mBox);

		for (UINT i = 0; i < 5; ++i)
		{
			mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder);
			mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder);
		}
	}

	// turn off tessellation

	// draw with reflection
	{
		// bind cube map SRV
		mContext->PSSetShaderResources(2, 1, mSky.mAlbedoSRV.GetAddressOf());

		DrawGameObject(&mSkull);

		for (UINT i = 0; i < 5; ++i)
		{
			mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere);
			mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere);
		}

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(2, 1, &NullSRV);
	}

	if (IsKeyPressed(GLFW_KEY_2))
	{
		mShadowMap.mDebugQuad.Draw(mContext, mShadowMap.GetSRV());
	}
	
	if (IsKeyPressed(GLFW_KEY_3))
	{
		mSSAO.mDebugQuad.Draw(mContext, mSSAO.GetAmbientMapSRV());
	}

	// draw sky
	DrawGameObject(&mSky);

	mSwapChain->Present(0, 0);
	
	// unbind shadow map and ambient map as SRV
	ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
	mContext->PSSetShaderResources(3, 2, NullSRV);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}