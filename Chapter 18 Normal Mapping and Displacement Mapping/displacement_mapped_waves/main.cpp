#include <D3DApp.h>

#include <cassert>
#include <string>
#include <array>
#include <sstream>

#include <d3dcompiler.h>

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

		float mHeightScale0;
		float mHeightScale1;
		float mMaxTessDistance;
		float mMinTessDistance;
		float mMinTessFactor;
		float mMaxTessFactor;
		XMFLOAT2 pad3;

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

		XMFLOAT4X4 mWavesNormalMapTransform0;
		XMFLOAT4X4 mWavesNormalMapTransform1;
		XMFLOAT4X4 mWavesDisplacementMapTransform0;
		XMFLOAT4X4 mWavesDisplacementMapTransform1;
	};

	static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerObjectCB;

	GameObject mSkull;
	GameObject mBox;
	//GameObject mGrid;
	GameObject mSphere;
	GameObject mCylinder;
	GameObject mSky;

	std::array<LightDirectional, 3> mLights;

	ID3D11SamplerState* mSamplerState;

	DynamicCubeMap mDynamicCubeMap;

	GameObject mWaves;

	ID3D11ShaderResourceView* mWavesNormalMapSRV0;
	ID3D11ShaderResourceView* mWavesNormalMapSRV1;

	XMFLOAT2 mWavesNormalMapOffset0;
	XMFLOAT2 mWavesNormalMapOffset1;
	XMFLOAT2 mWavesDisplacementMapOffset0;
	XMFLOAT2 mWavesDisplacementMapOffset1;

	XMFLOAT4X4 mWavesNormalMapTransform0;
	XMFLOAT4X4 mWavesNormalMapTransform1;
	XMFLOAT4X4 mWavesDisplacementMapTransform0;
	XMFLOAT4X4 mWavesDisplacementMapTransform1;
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr),
	mSamplerState(nullptr),
	mWavesNormalMapSRV0(nullptr),
	mWavesNormalMapSRV1(nullptr)
{
	mMainWindowTitle = "Ch18 Displacement Mapped Waves";

	//m4xMSAAEnabled = true;

	mLights[0].mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[0].mDiffuse = XMFLOAT4(0.7f, 0.7f, 0.6f, 1.0f);
	mLights[0].mSpecular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(0.707f, 0.0f, 0.707f);

	mLights[1].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse = XMFLOAT4(0.40f, 0.40f, 0.40f, 1.0f);
	mLights[1].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(0.0f, -0.707f, 0.707f);

	mLights[2].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(-0.57735f, -0.57735f, -0.57735f);

	mCamera.mPosition = XMFLOAT3(0.0f, 2.0f, -15.0f);
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

	std::wstring base = L""; // L"C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/";
	std::wstring proj = L""; // L"Chapter 18 Normal Mapping and Displacement Mapping/";

	// VS
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
		std::wstring path = base + proj + L"VS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mSkull.mVertexShader = shader;
		//mGrid.mVertexShader = shader;
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
			//mGrid.mInputLayout = layout;
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

		//mGrid.mHullShader = shader;
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

		//mGrid.mDomainShader = shader;
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

		//mGrid.mPixelShader = shader;
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

	//// build grid geometry
	//{
	//	GeometryGenerator::CreateGrid(20, 30, 50, 40, mGrid.mMesh);

	//	mGrid.mVertexStart = mSkull.mVertexStart + mSkull.mMesh.mVertices.size();
	//	mGrid.mIndexStart = mSkull.mIndexStart + mSkull.mMesh.mIndices.size();

	//	mGrid.mWorld = XMMatrixIdentity();
	//	mGrid.mTexCoordTransform = XMMatrixScaling(8, 10, 1);

	//	mGrid.mMaterial.mAmbient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	//	mGrid.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//	mGrid.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);
	//	mGrid.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	//	mGrid.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

	//	mGrid.mAlbedoSRV = mTextureManager.CreateSRV(L"stones.dds");
	//	mGrid.mNormalSRV = mTextureManager.CreateSRV(L"stones_n.dds");
	//}

	// waves
	{
		GeometryGenerator::CreateGrid(30, 30, 80, 80, mWaves.mMesh);

		mWaves.mVertexStart = mSkull.mVertexStart + mSkull.mMesh.mVertices.size();
		mWaves.mIndexStart = mSkull.mIndexStart + mSkull.mMesh.mIndices.size();

		mWaves.mMaterial.mAmbient  = XMFLOAT4(0.1f, 0.1f, 0.3f, 1.0f);
		mWaves.mMaterial.mDiffuse  = XMFLOAT4(0.4f, 0.4f, 0.7f, 1.0f);
		mWaves.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 128.0f);
		mWaves.mMaterial.mReflect  = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

		mWaves.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mWaves.mAlbedoSRV = mTextureManager.CreateSRV(L"stones.dds");
		mWaves.mNormalSRV = mTextureManager.CreateSRV(L"stones_n.dds");

		mWavesNormalMapSRV0 = mTextureManager.CreateSRV(L"waves0.dds");
		mWavesNormalMapSRV1 = mTextureManager.CreateSRV(L"waves1.dds");

		// VS
		{
			std::wstring path = base + proj + L"WavesVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mWaves.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mWaves.mInputLayout));
			}
		}

		// HS
		{
			std::wstring path = base + proj + L"WavesHS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "hs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateHullShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mWaves.mHullShader));
		}

		// DS
		{
			std::wstring path = base + proj + L"WavesDS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ds_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateDomainShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mWaves.mDomainShader));
		}

		// PS
		{
			std::wstring path = base + proj + L"WavesPS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
			defines.push_back({ "ENABLE_NORMAL_MAPPING",  "1" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			defines.push_back({ "ENABLE_LIGHTING",        "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mWaves.mPixelShader));
		}
	}

	// build box geometry
	{
		GeometryGenerator::CreateBox(1, 1, 1, mBox.mMesh);

		mBox.mVertexStart = mWaves.mVertexStart + mWaves.mMesh.mVertices.size();
		mBox.mIndexStart = mWaves.mIndexStart + mWaves.mMesh.mIndices.size();

		mBox.mWorld = XMMatrixScaling(3.0f, 1.0f, 3.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
		mBox.mTexCoordTransform = XMMatrixScaling(2, 1, 1);

		mBox.mMaterial.mAmbient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mBox.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		mBox.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mBox.mAlbedoSRV = mTextureManager.CreateSRV(L"floor.dds");
		mBox.mNormalSRV = mTextureManager.CreateSRV(L"floor_n.dds");
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

		mCylinder.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mCylinder.mAlbedoSRV = mTextureManager.CreateSRV(L"bricks.dds");
		mCylinder.mNormalSRV = mTextureManager.CreateSRV(L"bricks_n.dds");
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

		mSky.mAlbedoSRV = mTextureManager.CreateSRV(L"snowcube1024.dds");

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
			&mWaves,
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

	XMFLOAT3 CenterSpherePosition = XMFLOAT3(0.0f, 2.0f, 0.0f);
	mDynamicCubeMap.Init(mDevice, CenterSpherePosition);

	return true;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);
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

	mCamera.UpdateView();


	// scroll waves displacement and normal maps
	{
		// normal map 0
		{
			mWavesNormalMapOffset0.x += 0.05f * dt;
			mWavesNormalMapOffset0.y += 0.20f * dt;

			XMMATRIX S = XMMatrixScaling(22.0f, 22.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesNormalMapOffset0.x, mWavesNormalMapOffset0.y, 0.0f);
			XMStoreFloat4x4(&mWavesNormalMapTransform0, S * T);
		}

		// normal map 1
		{
			mWavesNormalMapOffset1.x -= 0.02f * dt;
			mWavesNormalMapOffset1.y += 0.05f * dt;

			XMMATRIX S = XMMatrixScaling(16.0f, 16.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesNormalMapOffset1.x, mWavesNormalMapOffset1.y, 0.0f);
			XMStoreFloat4x4(&mWavesNormalMapTransform1, S * T);
		}

		// displacement map 0
		{
			mWavesDisplacementMapOffset0.x += 0.01f * dt;
			mWavesDisplacementMapOffset0.y += 0.03f * dt;

			XMMATRIX S = XMMatrixScaling(2.0f, 2.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesDisplacementMapOffset0.x, mWavesDisplacementMapOffset0.y, 0.0f);
			XMStoreFloat4x4(&mWavesDisplacementMapTransform0, S * T);
		}

		// displacement map 1
		{
			mWavesDisplacementMapOffset1.x += 0.01f * dt;
			mWavesDisplacementMapOffset1.y += 0.03f * dt;

			XMMATRIX S = XMMatrixScaling(1.0f, 1.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesDisplacementMapOffset1.x, mWavesDisplacementMapOffset1.y, 0.0f);
			XMStoreFloat4x4(&mWavesDisplacementMapTransform1, S * T);
		}
	}
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	//mContext->RSSetViewports(1, &mViewport);
	//mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	auto SetPerFrameCB = [this](bool DrawWaves) -> void
	{
		PerFrameCB buffer;
		buffer.mLights[0] = mLights[0];
		buffer.mLights[1] = mLights[1];
		buffer.mLights[2] = mLights[2];
		buffer.mEyePositionW = mCamera.mPosition;
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
		buffer.mHeightScale0 = DrawWaves ? 0.4f : 0.07f;
		buffer.mHeightScale1 = DrawWaves ? 1.2f : 0;
		buffer.mMaxTessDistance = DrawWaves ? 4 : 1;
		buffer.mMinTessDistance = DrawWaves ? 30 : 25;
		buffer.mMinTessFactor = DrawWaves ? 2 : 1;
		buffer.mMaxTessFactor = DrawWaves ? 6 : 5;
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mViewProj, V * mCamera.mProj);

		mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->DSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * mCamera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexCoordTransform, obj->mTexCoordTransform);

		buffer.mWavesNormalMapTransform0 = mWavesNormalMapTransform0;
		buffer.mWavesNormalMapTransform1 = mWavesNormalMapTransform1;
		buffer.mWavesDisplacementMapTransform0 = mWavesDisplacementMapTransform0;
		buffer.mWavesDisplacementMapTransform1 = mWavesDisplacementMapTransform1;

		mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj, bool DrawWaves) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
		mContext->HSSetShader(obj->mHullShader.Get(), nullptr, 0);
		mContext->DSSetShader(obj->mDomainShader.Get(), nullptr, 0);
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

		// textures
		if (DrawWaves)
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);

			mContext->DSSetShaderResources(1, 1, &mWavesNormalMapSRV0);
			mContext->DSSetShaderResources(2, 1, &mWavesNormalMapSRV1);
			mContext->PSSetShaderResources(1, 1, &mWavesNormalMapSRV0);
			mContext->PSSetShaderResources(2, 1, &mWavesNormalMapSRV1);
		}
		else
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);
			mContext->DSSetShaderResources(1, 1, &obj->mNormalSRV);
			mContext->PSSetShaderResources(1, 1, &obj->mNormalSRV);
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
		ID3D11ShaderResourceView* const NullSRV[3] = { nullptr, nullptr, nullptr };
		mContext->PSSetShaderResources(0, 3, NullSRV);
	};

	SetPerFrameCB(false);

	// draw without reflection
	{
		DrawGameObject(&mBox, false);

		for (UINT i = 0; i < 5; ++i)
		{
			mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder, false);
			mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
			DrawGameObject(&mCylinder, false);
		}
	}

	// turn off tessellation

	// draw with reflection
	{
		// bind cube map SRV
		mContext->PSSetShaderResources(3, 1, &mSky.mAlbedoSRV);

		SetPerFrameCB(true);
		DrawGameObject(&mWaves, true);

		SetPerFrameCB(false);

		DrawGameObject(&mSkull, false);

		for (UINT i = 0; i < 5; ++i)
		{
			mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere, false);
			mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
			DrawGameObject(&mSphere, false);
		}

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(3, 1, &NullSRV);
	}

	// draw sky
	DrawGameObject(&mSky, false);

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}