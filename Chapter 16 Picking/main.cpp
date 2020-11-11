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
		XMFLOAT4X4 mWorldViewProj;
		GameObject::Material mMaterial;
		XMFLOAT4X4 mTexTransform;
	};

	ID3D11Buffer* mPerObjectCB;

	//GameObject mSkull;
	GameObject mCar;
	
	std::array<LightDirectional, 3> mLights;

	UINT mPickedTriangle;
	GameObject::Material mPickedMaterial;

	void OnMouseButton(GLFWwindow* window, int button, int action, int mods) override;
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr)
{
	mMainWindowTitle = "Ch16 Picking";

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

	mPickedTriangle = -1;

	mPickedMaterial.mAmbient = XMFLOAT4(0.0f, 0.8f, 0.4f, 1.0f);
	mPickedMaterial.mDiffuse = XMFLOAT4(0.0f, 0.8f, 0.4f, 1.0f);
	mPickedMaterial.mSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 16.0f);
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
	std::wstring proj = L"Chapter 16 Picking/";

	// VS
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
		std::wstring path = base + proj + L"VS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mCar.mVertexShader = shader;

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

			mCar.mInputLayout = layout;
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

		mCar.mPixelShader = shader;
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

	// build car geometry
	{
		GeometryGenerator::CreateCar(mCar.mMesh);

		mCar.mVertexStart = 0;

		mCar.mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 1, 0);

		mCar.mMaterial.mAmbient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		mCar.mMaterial.mDiffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mCar.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);

		// VB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mCar.mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			UpdateVertexBuffer(buffer, mCar);

			mCar.mVertexBuffer = buffer;
		}

		// IB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * (mCar.mMesh.mIndices.size() + mCar.mMesh.mIndices.size());
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			UpdateIndexBuffer(buffer, mCar);

			mCar.mIndexBuffer = buffer;
		}
	}

	return true;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);
}

void TestApp::OnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
	D3DApp::OnMouseButton(window, button, action, mods);

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		double sx, sy;
		glfwGetCursorPos(window, &sx, &sy);

		XMFLOAT4X4 P;
		XMStoreFloat4x4(&P, mCamera.mProj);

		float vx = (+2*sx/mMainWindowWidth  - 1) / P(0,0);
		float vy = (-2*sy/mMainWindowHeight + 1) / P(1,1);

		XMVECTOR RayOri = XMVectorSet(0, 0, 0, 1);
		XMVECTOR RayDir = XMVectorSet(vx, vy, 1, 0);

		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX InverseView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

		XMMATRIX InverseWorld = XMMatrixInverse(&XMMatrixDeterminant(mCar.mWorld), mCar.mWorld);

		XMMATRIX ToLocal = InverseView * InverseWorld;

		RayOri = XMVector3TransformCoord(RayOri, ToLocal);
		RayDir = XMVector3TransformNormal(RayDir, ToLocal);
		RayDir = XMVector3Normalize(RayDir);

		mPickedTriangle = -1;
		float T = 0;

		if (mCar.mMesh.mAABB.Intersects(RayOri, RayDir, T))
		{
			T = FLT_MAX;

			for (UINT i = 0; i < mCar.mMesh.mIndices.size(); i += 3)
			{
				UINT i0 = mCar.mMesh.mIndices.at(i + 0);
				UINT i1 = mCar.mMesh.mIndices.at(i + 1);
				UINT i2 = mCar.mMesh.mIndices.at(i + 2);

				XMVECTOR v0 = XMLoadFloat3(&mCar.mMesh.mVertices.at(i0).mPosition);
				XMVECTOR v1 = XMLoadFloat3(&mCar.mMesh.mVertices.at(i1).mPosition);
				XMVECTOR v2 = XMLoadFloat3(&mCar.mMesh.mVertices.at(i2).mPosition);

				float t = 0;

				if (TriangleTests::Intersects(RayOri, RayDir, v0, v1, v2, t))
				{
					if (t < T)
					{
						T = t;
						mPickedTriangle = i;
					}
				}

			} // for each triangle
		}
	}
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
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * mCamera.mProj);
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

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	SetPerFrameCB();

	DrawGameObject(&mCar);

	if (mPickedTriangle != -1)
	{
		mContext->RSSetState(nullptr);
		mContext->OMSetDepthStencilState(mLessEqualDSS.Get(), 0);

		{
			PerObjectCB buffer;
			XMStoreFloat4x4(&buffer.mWorld, mCar.mWorld);
			XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(mCar.mWorld));
			XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
			XMStoreFloat4x4(&buffer.mWorldViewProj, mCar.mWorld * V * mCamera.mProj);
			buffer.mMaterial = mPickedMaterial;
			XMStoreFloat4x4(&buffer.mTexTransform, mCar.mTexTransform);

			mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

			mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
			mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
		}

		mContext->DrawIndexed(3, mPickedTriangle, mCar.mVertexStart);

		mContext->OMSetDepthStencilState(nullptr, 0);
	}

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}