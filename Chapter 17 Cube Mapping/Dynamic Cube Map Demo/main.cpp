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

	void DrawScene(const CameraObject& camera, bool DrawCenterSphere);

	struct PerFrameCB
	{
		std::array<LightDirectional, 3> mLights;

		XMFLOAT3 mEyePositionW;
		float    pad1;

		float    mFogStart;
		float    mFogRange;
		XMFLOAT2 pad2;
		XMFLOAT4 mFogColor;
	};

	ID3D11Buffer* mPerFrameCB;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		GameObject::Material mMaterial;
		XMFLOAT4X4 mTexTransform;
	};

	ID3D11Buffer* mPerObjectCB;

	GameObject mSkull;
	GameObject mBox;
	GameObject mGrid;
	GameObject mSphere;
	GameObject mCylinder;
	GameObject mSky;
	GameObject mCenterSphere;

	std::array<LightDirectional, 3> mLights;

	ID3D11SamplerState* mSamplerState;

	DynamicCubeMap mDynamicCubeMap;
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr),
	mSamplerState(nullptr)
{
	mMainWindowTitle = "Ch17 Dynamic Cube Map Demo";

	//m4xMSAAEnabled = true;

	mLights[0].mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[0].mDiffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLights[0].mSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	mLights[1].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);
	mLights[1].mSpecular = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	mLights[2].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(0.0f, -0.707f, -0.707f);

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

	std::wstring base = L"C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/";
	std::wstring proj = L"Chapter 17 Cube Mapping/Dynamic Cube Map Demo/";

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
		mCenterSphere.mVertexShader = shader;

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA,   0},
			};

			Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

			mSkull.mInputLayout = layout;
			mGrid.mInputLayout = layout;
			mBox.mInputLayout = layout;
			mCylinder.mInputLayout = layout;
			mSphere.mInputLayout = layout;
			mCenterSphere.mInputLayout = layout;
		}
	}

	// PS
	{
		Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
		std::wstring path = base + proj + L"PS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_TEXTURE",        "1" });
		defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
		//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
		//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		defines.push_back({ "ENABLE_REFLECTION",     "0" });
		defines.push_back({ "ENABLE_FOG",            "0" });
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
		static_assert((sizeof(PerFrameCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

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
		static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

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

		mSkull.mMaterial.mAmbient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		mSkull.mMaterial.mDiffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mSkull.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mSkull.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",        "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",     "0" });
			defines.push_back({ "ENABLE_FOG",            "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSkull.mPixelShader));
		}
	}

	// build grid geometry
	{
		GeometryGenerator::CreateGrid(20, 30, 60, 40, mGrid.mMesh);

		mGrid.mVertexStart = mSkull.mVertexStart + mSkull.mMesh.mVertices.size();
		mGrid.mIndexStart = mSkull.mIndexStart + mSkull.mMesh.mIndices.size();

		mGrid.mWorld = XMMatrixIdentity();
		mGrid.mTexTransform = XMMatrixScaling(6.0f, 8.0f, 1.0f);

		mGrid.mMaterial.mAmbient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mGrid.mMaterial.mDiffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mGrid.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mGrid.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		CreateSRV(L"floor.dds", &mGrid.mSRV);
	}

	// build box geometry
	{
		GeometryGenerator::CreateBox(1, 1, 1, mBox.mMesh);

		mBox.mVertexStart = mGrid.mVertexStart + mGrid.mMesh.mVertices.size();
		mBox.mIndexStart = mGrid.mIndexStart + mGrid.mMesh.mIndices.size();

		mBox.mWorld = XMMatrixScaling(3.0f, 1.0f, 3.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f); // XMMatrixMultiply(boxScale, boxOffset));

		mBox.mMaterial.mAmbient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mBox.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mBox.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		CreateSRV(L"stone.dds", &mBox.mSRV);
	}

	// build cylinder geometry
	{
		GeometryGenerator::CreateCylinder(0.5f, 0.3f, 3, 20, 20, mCylinder.mMesh);

		mCylinder.mVertexStart = mBox.mVertexStart + mBox.mMesh.mVertices.size();
		mCylinder.mIndexStart = mBox.mIndexStart + mBox.mMesh.mIndices.size();

		mCylinder.mMaterial.mAmbient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mCylinder.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mCylinder.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mCylinder.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		CreateSRV(L"bricks.dds", &mCylinder.mSRV);
	}

	// build sphere geometry
	{
		GeometryGenerator::CreateSphere(0.5f, 3, mSphere.mMesh);

		mSphere.mVertexStart = mCylinder.mVertexStart + mCylinder.mMesh.mVertices.size();
		mSphere.mIndexStart = mCylinder.mIndexStart + mCylinder.mMesh.mIndices.size();

		mSphere.mMaterial.mAmbient = XMFLOAT4(1, 1, 1, 1.0f);
		mSphere.mMaterial.mDiffuse = XMFLOAT4(1, 1, 1, 1.0f);
		mSphere.mMaterial.mSpecular = XMFLOAT4(0.9f, 0.9f, 0.9f, 16.0f);
		mSphere.mMaterial.mReflect = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",        "1" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "1" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",     "0" });
			defines.push_back({ "ENABLE_FOG",            "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSphere.mPixelShader));
		}

		mSphere.mSRV = mBox.mSRV;
	}

	// build sky geometry
	{
		GeometryGenerator::CreateSphere(5000, 3, mSky.mMesh);

		mSky.mVertexStart = mSphere.mVertexStart + mSphere.mMesh.mVertices.size();
		mSky.mIndexStart = mSphere.mIndexStart + mSphere.mMesh.mIndices.size();

		mSky.mRasterizerState = mNoCullRS;
		mSky.mDepthStencilState = mLessEqualDSS;

		CreateSRV(L"grasscube1024.dds", &mSky.mSRV);

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

	// build center sphere geometry
	{
		GeometryGenerator::CreateSphere(0.5f, 3, mCenterSphere.mMesh);

		mCenterSphere.mVertexStart = mSky.mVertexStart + mSky.mMesh.mVertices.size();
		mCenterSphere.mIndexStart = mSky.mIndexStart + mSky.mMesh.mIndices.size();

		mCenterSphere.mWorld = XMMatrixScaling(2, 2, 2) * XMMatrixTranslation(0, 2, 0);

		mCenterSphere.mMaterial.mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
		mCenterSphere.mMaterial.mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
		mCenterSphere.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mCenterSphere.mMaterial.mReflect = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);

		mCenterSphere.mSRV = mBox.mSRV;

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "1" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "1" });
			//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mCenterSphere.mPixelShader));
		}
	}

	// create vertex and input buffers
	{
		std::array<GameObject*, 7> objects =
		{
			&mSkull,
			&mGrid,
			&mBox,
			&mCylinder,
			&mSphere,
			&mSky,
			&mCenterSphere
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

	// update skull position
	{
		XMMATRIX S = XMMatrixScaling(0.2f, 0.2f, 0.2f);				// scale
		XMMATRIX T = XMMatrixTranslation(3.0f, 2.0f, 0.0f);			// translate
		XMMATRIX LR = XMMatrixRotationY(2.0f * mTimer.TotalTime()); // local rotate
		XMMATRIX WR = XMMatrixRotationY(0.5f * mTimer.TotalTime()); // world rotate
		mSkull.mWorld = S * LR * T * WR;
	}
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	mContext->RSSetViewports(1, &mDynamicCubeMap.mViewport);

	for (UINT i = 0; i < 6; ++i)
	{
		mContext->ClearRenderTargetView(mDynamicCubeMap.mRTV[i], Colors::Silver);
		mContext->ClearDepthStencilView(mDynamicCubeMap.mDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

		mContext->OMSetRenderTargets(1, &mDynamicCubeMap.mRTV[i], mDynamicCubeMap.mDSV);

		DrawScene(mDynamicCubeMap.mCamera[i], false);
	}

	mContext->GenerateMips(mDynamicCubeMap.mSRV);

	mContext->RSSetViewports(1, &mViewport);
	mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	DrawScene(mCamera, true);

	mSwapChain->Present(0, 0);
}

void TestApp::DrawScene(const CameraObject& camera, bool DrawCenterSphere)
{
	auto SetPerFrameCB = [this, &camera]() -> void
	{
		PerFrameCB buffer;
		buffer.mLights[0] = mLights[0];
		buffer.mLights[1] = mLights[1];
		buffer.mLights[2] = mLights[2];
		buffer.mEyePositionW = camera.mPosition;
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);

		mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

		//mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	auto SetPerObjectCB = [this, &camera](GameObject* obj) -> void
	{
		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMMATRIX V = XMLoadFloat4x4(&camera.mView);
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * camera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexTransform);

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

		// textures
		if (obj->mSRV.Get())
		{
			mContext->PSSetShaderResources(0, 1, obj->mSRV.GetAddressOf());
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
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(0, 1, &NullSRV);
	};

	SetPerFrameCB();

	// draw without reflection

	DrawGameObject(&mSkull);
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

	// draw with reflection

	if (DrawCenterSphere)
	{
		// bind dynamic cube map SRV
		mContext->PSSetShaderResources(1, 1, &mDynamicCubeMap.mSRV);
		
		DrawGameObject(&mCenterSphere);

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(1, 1, &NullSRV);
	}

	// draw sky
	DrawGameObject(&mSky);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}