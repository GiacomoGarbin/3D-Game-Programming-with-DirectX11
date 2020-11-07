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
	
	GameObject mSkull;
	XMMATRIX mWorld; // ?

	struct InstancedData
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4   mColor;
	};

	ID3D11Buffer* mInstancedBuffer;

	std::vector<InstancedData> mInstancedData;

	UINT mVisibleObjectCount;
	bool mFrustumCullingEnabled;

	std::array<LightDirectional, 3> mLights;

	CameraObject mCamera;
};

TestApp::TestApp() :
	D3DApp(),
	mInstancedBuffer(nullptr)
{
	mMainWindowTitle = "Ch15 Instancing and Frustum Culling";

	//m4xMSAAEnabled = true;

	mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 1, 0);

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

	mSkull.mMaterial.mAmbient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
	mSkull.mMaterial.mDiffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	mSkull.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);

	mCamera.mPosition = XMFLOAT3(0.0f, 2.0f, -15.0f);
}

TestApp::~TestApp()
{
	SafeRelease(mInstancedBuffer);
}

bool TestApp::Init()
{
	if (!D3DApp::Init())
	{
		return false;
	}

	std::wstring base = L"C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/";
	std::wstring proj = L"Chapter 15 Instancing and Frustum Culling/";

	// build skull geometry

	// build instanced buffer

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
	mVisibleObjectCount = 0;

	if (mFrustumCullingEnabled)
	{

	}
	else
	{

	}

	mMainWindowTitle = "Ch15 Instancing and Frustum Culling | Visible Objects " + std::to_string(mVisibleObjectCount) + "/" + std::to_string(mInstancedData.size());
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

		mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->HSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->DSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->GSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	static auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		//XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * mView * mProj);
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * mCamera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexTransform);

		mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->HSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->DSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->GSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
	};

	static auto DrawGameObject = [this](GameObject* obj) -> void
	{
		UINT stride = sizeof(GeometryGenerator::Vertex);
		UINT offset = 0;

		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
		mContext->HSSetShader(obj->mHullShader.Get(), nullptr, 0);
		mContext->DSSetShader(obj->mDomainShader.Get(), nullptr, 0);
		mContext->GSSetShader(obj->mGeometryShader.Get(), nullptr, 0);
		mContext->PSSetShader(obj->mPixelShader.Get(), nullptr, 0);

		// input layout
		mContext->IASetInputLayout(obj->mInputLayout.Get());

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
		mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// constant buffer per object
		SetPerObjectCB(obj);

		// textures
		if (obj->mSRV.Get())
		{
			mContext->PSSetShaderResources(0, 1, obj->mSRV.GetAddressOf());
		}

		//// dispacement map
		//if (obj == &mWaves)
		//{
		//	mContext->VSSetShaderResources(1, 1, &mCurrTextureSRV);
		//}

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(obj->mRasterizerState.Get());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

		// draw call
		if (obj->mIndexBuffer)
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

		//if (obj == &mWaves)
		//{
		//	mContext->PSSetShaderResources(1, 1, &NullSRV);
		//}
	};

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	SetPerFrameCB();

	for (GameObject* obj : mGameObjects)
	{
		DrawGameObject(obj);
	}

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}