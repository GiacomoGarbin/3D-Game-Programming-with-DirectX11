#include <D3DApp.h>

#include <cassert>
#include <string>
#include <array>
#include <sstream>

#include <d3dcompiler.h>

#include <DDSTextureLoader.h>

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
	};

	ID3D11Buffer* mPerFrameCB;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mViewProj;
		GameObject::Material mMaterial;
		XMFLOAT4X4 mTexTransform;
	};

	ID3D11Buffer* mPerObjectCB;

	GameObject mSkull;
	//XMMATRIX mWorld;

	//UINT mVisibleObjectCount;
	bool mFrustumCullingEnabled;

	std::array<LightDirectional, 3> mLights;
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr)
{
	mMainWindowTitle = "Ch15 Instancing and Frustum Culling";

	//m4xMSAAEnabled = true;

	//mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 1, 0);

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
}

bool TestApp::Init()
{
	if (!D3DApp::Init())
	{
		return false;
	}

	std::wstring base = L"C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/";
	std::wstring proj = L"Chapter 15 Instancing and Frustum Culling/";

	// VS
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
		std::wstring path = base + proj + L"VS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mSkull.mVertexShader = shader;

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA,   0},
				{"WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{"WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{"WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{"WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			};

			Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

			mSkull.mInputLayout = layout;
		}
	}
	  
	// PS
	{
		Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
		std::wstring path = base + proj + L"PS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		//defines.push_back({ "ENABLE_TEXTURE",        "1" });
		//defines.push_back({ "ENABLE_ALPHA_CLIPPING", "1" });
		//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		//defines.push_back({ "ENABLE_FOG",            "1" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mSkull.mPixelShader = shader;
	} // PS

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

	static auto UpdateBuffer = [this](Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, UINT start, UINT count, UINT bytes, const void* data) -> void
	{
		D3D11_BOX region;
		region.left = bytes * start;
		region.right = bytes * (start + count);
		region.top = 0;
		region.bottom = 1;
		region.front = 0;
		region.back = 1;

		mContext->UpdateSubresource(*buffer.GetAddressOf(), 0, &region, data, 0, 0);
	};

	static auto UpdateVertexBuffer = [](Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, const GameObject& obj) -> void
	{
		UpdateBuffer(buffer, obj.mVertexStart, obj.mMesh.mVertices.size(), sizeof(GeometryGenerator::Vertex), obj.mMesh.mVertices.data());
	};

	static auto UpdateIndexBuffer = [](Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer, const GameObject& obj) -> void
	{
		UpdateBuffer(buffer, obj.mIndexStart, obj.mMesh.mIndices.size(), sizeof(UINT), obj.mMesh.mIndices.data());
	};

	// build skull geometry
	{
		GeometryGenerator::CreateModel("skull.txt", mSkull.mMesh);

		mSkull.mVertexStart = 0;

		mSkull.mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 1, 0);

		mSkull.mMaterial.mAmbient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		mSkull.mMaterial.mDiffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mSkull.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);

		// VB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mSkull.mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			UpdateVertexBuffer(buffer, mSkull);

			mSkull.mVertexBuffer = buffer;
		}

		// IB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * (mSkull.mMesh.mIndices.size() + mSkull.mMesh.mIndices.size());
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			UpdateIndexBuffer(buffer, mSkull);

			mSkull.mIndexBuffer = buffer;
		}
	}

	// build instanced buffer
	{
		UINT rows = 5;
		UINT cols = 5;
		UINT cuts = 5;
		
		float width  = 200;
		float height = 200;
		float depth  = 200;
		
		float x = -0.5f * width;
		float y = -0.5f * height;
		float z = -0.5f * depth;

		float dx = width  / (cols - 1);
		float dy = height / (rows - 1);
		float dz = width  / (cuts - 1);

		mSkull.mInstances.resize(rows * cols * cuts);

		for (UINT row = 0; row < rows; ++row)
		{
			for (UINT col = 0; col < cols; ++col)
			{
				for (UINT cut = 0; cut < cuts; ++cut)
				{
					mSkull.mInstances.at(cut * cols * rows + row * cols + col).mWorld = XMFLOAT4X4
					(
						1, 0, 0, 0,
						0, 1, 0, 0,
						0, 0, 1, 0,
						x + dx * col, y + dy * row, z + dz * cut, 1
					);

					mSkull.mInstances.at(cut* cols* rows + row * cols + col).mColor.x = GameMath::RandNorm();
					mSkull.mInstances.at(cut* cols* rows + row * cols + col).mColor.y = GameMath::RandNorm();
					mSkull.mInstances.at(cut* cols* rows + row * cols + col).mColor.z = GameMath::RandNorm();
					mSkull.mInstances.at(cut* cols* rows + row * cols + col).mColor.w = 1;
				}
			}
		}


		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(GameObject::InstancedData) * mSkull.mInstances.size();
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mSkull.mInstancedBuffer));
	}

	return true;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);

	// ComputeFrustumFromProjection ?
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

	if (IsKeyPressed(GLFW_KEY_1))
	{
		mFrustumCullingEnabled = !mFrustumCullingEnabled;
	}

	// frustum culling

	mCamera.UpdateView();

	mSkull.mVisibleInstanceCount = 0;

	if (mFrustumCullingEnabled)
	{

	}
	else
	{
		D3D11_MAPPED_SUBRESOURCE MappedData;

		HR(mContext->Map(mSkull.mInstancedBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData));

		GameObject::InstancedData* instance = reinterpret_cast<GameObject::InstancedData*>(MappedData.pData);

		for (std::size_t i = 0; i < mSkull.mInstances.size(); i++)
		{
			instance[i] = mSkull.mInstances[i];
			mSkull.mVisibleInstanceCount++;
		}

		mContext->Unmap(mSkull.mInstancedBuffer, 0);
	}

	mMainWindowTitle = "Ch15 Instancing and Frustum Culling | Visible Objects " + std::to_string(mSkull.mVisibleInstanceCount) + "/" + std::to_string(mSkull.mInstances.size());
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	static auto SetPerFrameCB = [this]() -> void
	{
		PerFrameCB buffer;
		buffer.mLights[0] = mLights[0];
		buffer.mLights[1] = mLights[1];
		buffer.mLights[2] = mLights[2];
		//XMStoreFloat3(&buffer.mEyePositionW, mEyePosition);
		buffer.mEyePositionW = mCamera.mPosition;
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);

		mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

		//mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	static auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mViewProj, V * mCamera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexTransform);

		mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
	};

	static auto DrawGameObject = [this](GameObject* obj) -> void
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
		ID3D11ShaderResourceView* const NullSRV = nullptr;
		mContext->PSSetShaderResources(0, 1, &NullSRV);
	};

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	SetPerFrameCB();

	DrawGameObject(&mSkull);

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}