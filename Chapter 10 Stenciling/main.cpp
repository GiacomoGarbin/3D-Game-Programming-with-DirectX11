#include <D3DApp/D3DApp.h>

#include <cassert>
#include <string>
#include <array>
#include <sstream>

#include <d3dcompiler.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

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

	ID3D11InputLayout* mInputLayout;

	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProj;

	XMVECTOR mEyePosition;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		GameObject::Material mMaterial;
		XMFLOAT4X4 mTexTransform;
	};

	struct PerFrameCB
	{
		//LightDirectional mLightDirectional;
		//LightPoint       mLightPoint;
		//LightSpot        mLightSpot;
		LightDirectional mLights[3];

		XMFLOAT3 mEyePositionW;
		float    pad1;

		float    mFogStart;
		float    mFogRange;
		XMFLOAT2 pad2;
		XMFLOAT4 mFogColor;
	};

	ID3D11Buffer* mCbPerObject;
	ID3D11Buffer* mCbPerFrame;

	//ID3D11VertexShader* mVertexShader;
	//ID3D11PixelShader* mPixelShader;

	GameObject mFloor;
	GameObject mWall;
	GameObject mMirror;
	GameObject mSkull;

	std::vector<GameObject*> mObjects;

	//LightDirectional mDirLight;
	//LightPoint mPointLight;
	//LightSpot mSpotLight;
	LightDirectional mLights[3];

	ID3D11SamplerState* mSamplerState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mNoCullRS;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mCullClockwiseRS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mNoRenderTargetWritesBS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mTransparentBS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mMarkMirrorDSS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDrawReflectionDSS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mNoDoubleBlendDSS;
};

TestApp::TestApp()
	: D3DApp()
{
	mMainWindowTitle = "Ch10 Stenciling";

	mWorld = XMMatrixIdentity();
	mView = XMMatrixIdentity();
	mProj = XMMatrixIdentity();

	//mDirLight.mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	//mDirLight.mDiffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	//mDirLight.mSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	//mDirLight.mDirection = XMFLOAT3(+0.57735f, -0.57735f, +0.57735f);

	//mPointLight.mAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	//mPointLight.mDiffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	//mPointLight.mSpecular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	//mPointLight.mAttenuation = XMFLOAT3(0.0f, 0.1f, 0.0f);
	//mPointLight.mRange = 25;

	//mSpotLight.mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	//mSpotLight.mDiffuse = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
	//mSpotLight.mSpecular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//mSpotLight.mAttenuation = XMFLOAT3(1.0f, 0.0f, 0.0f);
	//mSpotLight.mCone = 96;
	//mSpotLight.mRange = 10000;

	mLights[0].mAmbient   = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[0].mDiffuse   = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLights[0].mSpecular  = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	mLights[1].mAmbient   = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse   = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);
	mLights[1].mSpecular  = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	mLights[2].mAmbient   = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse   = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mSpecular  = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(0.0f, -0.707f, -0.707f);
}

TestApp::~TestApp()
{
	SafeRelease(mInputLayout);
	SafeRelease(mCbPerObject);
	SafeRelease(mCbPerFrame);
	//SafeRelease(mVertexShader);
	//SafeRelease(mPixelShader);
	SafeRelease(mSamplerState);
	SafeRelease((*mCullClockwiseRS.GetAddressOf()));
	SafeRelease((*mNoCullRS.GetAddressOf()));
	SafeRelease((*mNoRenderTargetWritesBS.GetAddressOf()));
	SafeRelease((*mTransparentBS.GetAddressOf()));
	SafeRelease((*mMarkMirrorDSS.GetAddressOf()));
	SafeRelease((*mDrawReflectionDSS.GetAddressOf()));
	SafeRelease((*mNoDoubleBlendDSS.GetAddressOf()));
}

bool TestApp::Init()
{
	if (D3DApp::Init())
	{
		ID3DBlob* pVertexShaderCode;

		// vertex and pixel shaders
		{
			Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			pVertexShaderCode = pCode;

			mFloor.mVertexShader = shader;
			mWall.mVertexShader = shader;
			mMirror.mVertexShader = shader;
			mSkull.mVertexShader = shader;
		} // vertex shader

		// pixel shader
		{
			Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "USE_TEXTURE", "1" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(L"PixelShader.hlsl", defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mFloor.mPixelShader = shader;
			mWall.mPixelShader = shader;
			mMirror.mPixelShader = shader;
		} // pixel shader

		// pixel shader skull
		{
			Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "USE_TEXTURE", "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(L"PixelShader.hlsl", defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mSkull.mPixelShader = shader;
		} // pixel shader

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &mInputLayout));

			mContext->IASetInputLayout(mInputLayout);
		} // input layout

		// primitive topology
		mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// game objects
		{
			// floor
			{
				auto& v = mFloor.mMesh.mVertices;

				v.resize(1 * 2 * 3);

				v[0] = GeometryGenerator::Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f);
				v[1] = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
				v[2] = GeometryGenerator::Vertex(7.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f);

				v[3] = GeometryGenerator::Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f);
				v[4] = GeometryGenerator::Vertex(7.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f);
				v[5] = GeometryGenerator::Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f);

				mFloor.mVertexStart = 0;

				mFloor.mMaterial.mAmbient  = XMFLOAT4(0.5f, 0.5f, 0.5f,  1.0f);
				mFloor.mMaterial.mDiffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f,  1.0f);
				mFloor.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

				mObjects.push_back(&mFloor);
			}

			// wall
			{
				auto& v = mWall.mMesh.mVertices;

				v.resize(3 * 2 * 3);

				v[0]  = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
				v[1]  = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
				v[2]  = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f);

				v[3]  = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
				v[4]  = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f);
				v[5]  = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f);

				v[6]  = GeometryGenerator::Vertex( 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
				v[7]  = GeometryGenerator::Vertex( 2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
				v[8]  = GeometryGenerator::Vertex( 7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f);

				v[9]  = GeometryGenerator::Vertex( 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
				v[10] = GeometryGenerator::Vertex( 7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f);
				v[11] = GeometryGenerator::Vertex( 7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f);

				v[12] = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
				v[13] = GeometryGenerator::Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
				v[14] = GeometryGenerator::Vertex( 7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f);

				v[15] = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
				v[16] = GeometryGenerator::Vertex( 7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f);
				v[17] = GeometryGenerator::Vertex( 7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f);

				mWall.mVertexStart = mFloor.mVertexStart + mFloor.mMesh.mVertices.size();

				mWall.mMaterial.mAmbient  = mFloor.mMaterial.mAmbient;
				mWall.mMaterial.mDiffuse  = mFloor.mMaterial.mDiffuse;
				mWall.mMaterial.mSpecular = mFloor.mMaterial.mSpecular;

				mObjects.push_back(&mWall);
			}

			// mirror
			{
				auto& v = mMirror.mMesh.mVertices;

				v.resize(1 * 2 * 3);

				v[0] = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
				v[1] = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
				v[2] = GeometryGenerator::Vertex( 2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);

				v[3] = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
				v[4] = GeometryGenerator::Vertex( 2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
				v[5] = GeometryGenerator::Vertex( 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

				mMirror.mVertexStart = mWall.mVertexStart + mWall.mMesh.mVertices.size();

				mMirror.mMaterial.mAmbient  = mFloor.mMaterial.mAmbient;
				mMirror.mMaterial.mDiffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
				mMirror.mMaterial.mSpecular = mFloor.mMaterial.mSpecular;

				mObjects.push_back(&mMirror);
			}

			// skull
			{
				GeometryGenerator::CreateModel("skull.txt", mSkull.mMesh);
				
				mSkull.mVertexStart = 0;

				mSkull.mWorld = XMMatrixRotationY(XM_PIDIV2) * XMMatrixScaling(0.45f, 0.45f, 0.45f) * XMMatrixTranslation(0.0f, 1.0f, -5.0f);

				mSkull.mMaterial.mAmbient  = XMFLOAT4(0.5f, 0.5f, 0.5f,  1.0f);
				mSkull.mMaterial.mDiffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f,  1.0f);
				mSkull.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

				mObjects.push_back(&mSkull);
			}
		} // game objects

		// vertex buffer floor, wall and mirror
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * (mFloor.mMesh.mVertices.size() + mWall.mMesh.mVertices.size() + mMirror.mMesh.mVertices.size());
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			static auto UpdateBuffer = [this, &buffer](GameObject& obj) -> void
			{
				D3D11_BOX region;
				region.left = sizeof(GeometryGenerator::Vertex) * obj.mVertexStart;
				region.right = sizeof(GeometryGenerator::Vertex) * (obj.mVertexStart + obj.mMesh.mVertices.size());
				region.top = 0;
				region.bottom = 1;
				region.front = 0;
				region.back = 1;

				mContext->UpdateSubresource(*buffer.GetAddressOf(), 0, &region, obj.mMesh.mVertices.data(), 0, 0);
			};

			UpdateBuffer(mFloor);
			UpdateBuffer(mWall);
			UpdateBuffer(mMirror);

			mFloor.mVertexBuffer = buffer;
			mWall.mVertexBuffer = buffer;
			mMirror.mVertexBuffer = buffer;

		} // vertex buffer

		// vertex buffer skull
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mSkull.mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mSkull.mMesh.mVertices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mSkull.mVertexBuffer));
		} // vertex buffer

		// index buffer skull
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * mSkull.mMesh.mIndices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mSkull.mMesh.mIndices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mSkull.mIndexBuffer));
		} // index buffer

		// constant buffer per object
		{
			static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(PerObjectCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(mDevice->CreateBuffer(&desc, nullptr, &mCbPerObject));

			mContext->VSSetConstantBuffers(0, 1, &mCbPerObject);
			mContext->PSSetConstantBuffers(0, 1, &mCbPerObject);
		} // constant buffer per object

		// constant buffer per frame
		{
			static_assert((sizeof(PerFrameCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(PerFrameCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(mDevice->CreateBuffer(&desc, nullptr, &mCbPerFrame));

			mContext->PSSetConstantBuffers(1, 1, &mCbPerFrame);
		} // constant buffer per frame

		//// rasterizer state
		//{
		//	D3D11_RASTERIZER_DESC desc;
		//	desc.FillMode = D3D11_FILL_SOLID;
		//	desc.CullMode = D3D11_CULL_BACK;
		//	desc.FrontCounterClockwise = false;
		//	desc.DepthBias = 0;
		//	desc.DepthBiasClamp = 0;
		//	desc.SlopeScaledDepthBias = 0;
		//	desc.DepthClipEnable = true;
		//	desc.ScissorEnable = false;
		//	desc.MultisampleEnable = false;
		//	desc.AntialiasedLineEnable = false;

		//	HR(mDevice->CreateRasterizerState(&desc, &mRasterizerState));

		//	mContext->RSSetState(mRasterizerState);
		//} // rasterizer state

		//// rasterizer state floor
		//{
		//	D3D11_RASTERIZER_DESC desc;
		//	desc.FillMode = D3D11_FILL_SOLID;
		//	desc.CullMode = D3D11_CULL_BACK;
		//	desc.FrontCounterClockwise = false;
		//	desc.DepthBias = 0;
		//	desc.DepthBiasClamp = 0;
		//	desc.SlopeScaledDepthBias = 0;
		//	desc.DepthClipEnable = true;
		//	desc.ScissorEnable = false;
		//	desc.MultisampleEnable = false;
		//	desc.AntialiasedLineEnable = false;

		//	Microsoft::WRL::ComPtr<ID3D11RasterizerState> p;
		//	HR(mDevice->CreateRasterizerState(&desc, &p));

		//	mFloor.mRasterizerState = p;
		//} // rasterizer state

		//// rasterizer state box
		//{
		//	D3D11_RASTERIZER_DESC desc;
		//	desc.FillMode = D3D11_FILL_SOLID;
		//	desc.CullMode = D3D11_CULL_NONE;
		//	desc.FrontCounterClockwise = false;
		//	desc.DepthBias = 0;
		//	desc.DepthBiasClamp = 0;
		//	desc.SlopeScaledDepthBias = 0;
		//	desc.DepthClipEnable = true;
		//	desc.ScissorEnable = false;
		//	desc.MultisampleEnable = false;
		//	desc.AntialiasedLineEnable = false;

		//	HR(mDevice->CreateRasterizerState(&desc, &mBox.mRasterizerState));
		//} // rasterizer state

		// NoCullRS
		{
			D3D11_RASTERIZER_DESC desc;
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_NONE;
			desc.FrontCounterClockwise = false;
			desc.DepthBias = 0;
			desc.DepthBiasClamp = 0;
			desc.SlopeScaledDepthBias = 0;
			desc.DepthClipEnable = true;
			desc.ScissorEnable = false;
			desc.MultisampleEnable = false;
			desc.AntialiasedLineEnable = false;

			HR(mDevice->CreateRasterizerState(&desc, &mNoCullRS));
		} // CullClockwiseRS

		// CullClockwiseRS
		{
			D3D11_RASTERIZER_DESC desc;
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_BACK;
			desc.FrontCounterClockwise = true;
			desc.DepthBias = 0;
			desc.DepthBiasClamp = 0;
			desc.SlopeScaledDepthBias = 0;
			desc.DepthClipEnable = true;
			desc.ScissorEnable = false;
			desc.MultisampleEnable = false;
			desc.AntialiasedLineEnable = false;

			HR(mDevice->CreateRasterizerState(&desc, &mCullClockwiseRS));
		} // CullClockwiseRS

		// NoRenderTargetWritesBS
		{
			D3D11_BLEND_DESC desc;
			desc.AlphaToCoverageEnable = false;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = 0;

			HR(mDevice->CreateBlendState(&desc, &mNoRenderTargetWritesBS));
		} // NoRenderTargetWritesBS

		// TransparentBS
		{
			D3D11_BLEND_DESC desc;
			desc.AlphaToCoverageEnable = false;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HR(mDevice->CreateBlendState(&desc, &mTransparentBS));
		} // TransparentBS

		// MarkMirrorDSS
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			desc.DepthEnable = true;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D11_COMPARISON_LESS;
			desc.StencilEnable = true;
			desc.StencilReadMask = 0xff;
			desc.StencilWriteMask = 0xff;
			desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
			desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			HR(mDevice->CreateDepthStencilState(&desc, &mMarkMirrorDSS));
		} // MarkMirrorDSS

		// DrawReflectionDSS
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			desc.DepthEnable = true;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_LESS;
			desc.StencilEnable = true;
			desc.StencilReadMask = 0xff;
			desc.StencilWriteMask = 0xff;
			desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;

			HR(mDevice->CreateDepthStencilState(&desc, &mDrawReflectionDSS));
		} // DrawReflectionDSS

		// NoDoubleBlendDSS
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			desc.DepthEnable = true;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_LESS;
			desc.StencilEnable = true;
			desc.StencilReadMask = 0xff;
			desc.StencilWriteMask = 0xff;
			desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
			desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;

			HR(mDevice->CreateDepthStencilState(&desc, &mNoDoubleBlendDSS));
		} // NoDoubleBlendDSS

		// shader resource view
		{
			static auto CreateTexture = [this](std::wstring name, ID3D11ShaderResourceView** view) -> void
			{
				std::wstring base = L"C:/Users/D3PO/source/repos/DirectX11Tutorial/textures/";
				std::wstring path = base + name;

				ID3D11Resource* resource = nullptr;

				HR(CreateDDSTextureFromFile(mDevice, path.c_str(), &resource, view));

				// check created texture
				{
					ID3D11Texture2D* texture = nullptr;
					HR(resource->QueryInterface(IID_ID3D11Texture2D, (void**)&texture));

					D3D11_TEXTURE2D_DESC desc;
					texture->GetDesc(&desc);
				}
			};

			CreateTexture(L"checkboard.dds", &mFloor.mSRV);
			CreateTexture(L"brick01.dds", &mWall.mSRV);
			CreateTexture(L"ice.dds", &mMirror.mSRV);

			CreateTexture(L"grass.dds", &mSkull.mSRV);
		} // shader resource view

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
		} // sampler state

		return true;
	} // if D3DApp::Init

	return false;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);
	mProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, AspectRatio(), 1, 1000);
}

void TestApp::UpdateScene(float dt)
{
	float x = mRadius * std::sin(mPhi) * std::cos(mTheta);
	float y = mRadius * std::cos(mPhi);
	float z = mRadius * std::sin(mPhi) * std::sin(mTheta);

	mEyePosition = XMVectorSet(x, y, z, 1);
	XMVECTOR FocusPosition = XMVectorSet(0, 0, 0, 1); // XMVectorZero();
	XMVECTOR UpDirection = XMVectorSet(0, 1, 0, 0);

	mView = XMMatrixLookAtLH(mEyePosition, FocusPosition, UpDirection);

	//static float BaseTime = 0;

	//if ((mTimer.TotalTime() - BaseTime) >= 0.25f)
	//{
	//	BaseTime += 0.25f;

	//	UINT i = 5 + rand() % (mWavesGenerator.mRows - 10);
	//	UINT j = 5 + rand() % (mWavesGenerator.mCols - 10);

	//	float r = GameMath::RandNorm(0.5f, 1.0f);

	//	mWavesGenerator.Disturb(i, j, r);
	//}

	//mWavesGenerator.Update(dt);

	//D3D11_MAPPED_SUBRESOURCE MappedData;

	//HR(mContext->Map(mWaves.mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData));

	//GeometryGenerator::Vertex* vertex = reinterpret_cast<GeometryGenerator::Vertex*>(MappedData.pData);

	//for (std::size_t i = 0; i < mWaves.mMesh.mVertices.size(); i++)
	//{
	//	vertex[i] = mWavesGenerator[i];
	//}

	//mContext->Unmap(mWaves.mVertexBuffer, 0);

	//auto GetHillHeight = [](float x, float z) -> float
	//{
	//	return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
	//};

	//mPointLight.mPosition.x = 70 * std::cos(0.2f * mTimer.TotalTime());
	//mPointLight.mPosition.z = 70 * std::sin(0.2f * mTimer.TotalTime());
	//mPointLight.mPosition.y = std::max(GetHillHeight(mPointLight.mPosition.x, mPointLight.mPosition.z), -3.0f) + 10;

	//XMStoreFloat3(&mSpotLight.mPosition, EyePosition);
	//XMStoreFloat3(&mSpotLight.mDirection, XMVector3Normalize(FocusPosition - EyePosition));

	//static XMFLOAT2 TexOffset = XMFLOAT2(0, 0);

	//TexOffset.x += 0.10f * dt;
	//TexOffset.y += 0.05f * dt;

	//mWaves.mTexTransform = XMMatrixScaling(5, 5, 0) * XMMatrixTranslation(TexOffset.x, TexOffset.y, 0);
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
		XMStoreFloat3(&buffer.mEyePositionW, mEyePosition);
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
		mContext->UpdateSubresource(mCbPerFrame, 0, 0, &buffer, 0, 0);
	};

	static auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * mView * mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexTransform);
		mContext->UpdateSubresource(mCbPerObject, 0, 0, &buffer, 0, 0);
	};

	static auto DrawGameObject = [this](GameObject* obj) -> void
	{
		UINT stride = sizeof(GeometryGenerator::Vertex);
		UINT offset = 0;

		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// vertex and pixel shader
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
		mContext->PSSetShader(obj->mPixelShader.Get(), nullptr, 0);

		// vertex and index buffers
		mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
		mContext->IASetIndexBuffer(obj->mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// constant buffer per object
		SetPerObjectCB(obj);

		// textures
		mContext->PSSetShaderResources(0, 1, &obj->mSRV);

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(obj->mRasterizerState.Get());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

		// draw call
		if (obj->mIndexBuffer)
		{
			mContext->DrawIndexed(obj->mMesh.mIndices.size(), 0, 0);
		}
		else
		{
			mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
		}
	};

	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	SetPerFrameCB();

	//for (GameObject* obj : mObjects)
	//{
	//}

	// draw floor
	mFloor.mRasterizerState = mNoCullRS;
	mFloor.mDepthStencilState = nullptr;
	mFloor.mStencilRef = 0;
	DrawGameObject(&mFloor);
	
	// draw wall
	mWall.mRasterizerState = mNoCullRS;
	DrawGameObject(&mWall);

	// cache mirror texture and material
	auto CacheMirrorSRV = mMirror.mSRV;
	auto CacheMirrorMaterial = mMirror.mMaterial;

	// set mirror's back wall texture and material
	mMirror.mSRV = mWall.mSRV;
	mMirror.mTexTransform = XMMatrixScaling(2, 2, 0) * XMMatrixTranslation(0.5f, 0, 0);
	mMirror.mMaterial = mWall.mMaterial;

	// draw mirror's back wall
	mMirror.mRasterizerState = mCullClockwiseRS;
	mMirror.mBlendState = nullptr;
	DrawGameObject(&mMirror);

	// restore mirror texture and material
	mMirror.mSRV = CacheMirrorSRV;
	mMirror.mTexTransform = XMMatrixIdentity();
	mMirror.mMaterial = CacheMirrorMaterial;

	// draw skull
	mSkull.mRasterizerState = nullptr;
	mSkull.mBlendState = nullptr;
	mSkull.mDepthStencilState = nullptr;
	mSkull.mStencilRef = 0;
	DrawGameObject(&mSkull);

	// draw mirror on stencil only
	mMirror.mRasterizerState = nullptr;
	mMirror.mBlendState = mNoRenderTargetWritesBS;
	mMirror.mDepthStencilState = mMarkMirrorDSS;
	mMirror.mStencilRef = 1;
	DrawGameObject(&mMirror);

	// mirror plane and matrix
	XMVECTOR MirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX MirrorMatrix = XMMatrixReflect(MirrorPlane);

	XMFLOAT3 CacheLightDirections[3];
	for (std::size_t i = 0; i < 3; ++i)
	{
		// cache light direction
		CacheLightDirections[i] = mLights[i].mDirection;
		// reflect light direction
		XMVECTOR OldDir = XMLoadFloat3(&CacheLightDirections[i]);
		XMVECTOR NewDir = XMVector3TransformNormal(OldDir, MirrorMatrix);
		XMStoreFloat3(&mLights[i].mDirection, NewDir);
	}

	// update lights
	SetPerFrameCB();

	// reflect floor world matrix
	mFloor.mWorld = MirrorMatrix;

	// draw mirrored floor
	mFloor.mRasterizerState = mCullClockwiseRS;
	mFloor.mDepthStencilState = mDrawReflectionDSS;
	mFloor.mStencilRef = 1;
	DrawGameObject(&mFloor);

	// restore floor world matrix
	mFloor.mWorld = XMMatrixIdentity();

	// cache skull world matrix
	XMMATRIX CacheSkullWorld = mSkull.mWorld;
	// reflect skull world matrix
	mSkull.mWorld = CacheSkullWorld * MirrorMatrix;

	// draw mirrored skull
	mSkull.mRasterizerState = mCullClockwiseRS;
	mSkull.mDepthStencilState = mDrawReflectionDSS;
	mSkull.mStencilRef = 1;
	DrawGameObject(&mSkull);

	// shadow plane and matrix
	XMVECTOR ShadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMMATRIX ShadowMatrix = XMMatrixShadow(ShadowPlane, -XMLoadFloat3(&mLights[0].mDirection));

	// set mirrored shadow world matrix
	mSkull.mWorld = CacheSkullWorld * ShadowMatrix * XMMatrixTranslation(0, 0.001f, 0) * MirrorMatrix;
	
	// cache skull material
	auto CacheSkullMaterial = mSkull.mMaterial;
	// set shadow material
	mSkull.mMaterial.mAmbient  = XMFLOAT4(0.0f, 0.0f, 0.0f,  1.0f);
	mSkull.mMaterial.mDiffuse  = XMFLOAT4(0.0f, 0.0f, 0.0f,  0.5f);
	mSkull.mMaterial.mSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 16.0f);

	// draw mirrored skull shadow
	mSkull.mRasterizerState = nullptr;
	mSkull.mBlendState = mTransparentBS;
	mSkull.mDepthStencilState = mNoDoubleBlendDSS;
	mSkull.mStencilRef = 1;
	DrawGameObject(&mSkull);

	for (std::size_t i = 0; i < 3; ++i)
	{
		// restore light direction
		mLights[i].mDirection = CacheLightDirections[i];
	}

	// draw mirror
	mMirror.mBlendState = mTransparentBS;
	mMirror.mDepthStencilState = nullptr;
	mMirror.mStencilRef = 0;
	DrawGameObject(&mMirror);

	// set shadow world matrix
	mSkull.mWorld = CacheSkullWorld * ShadowMatrix * XMMatrixTranslation(0, 0.001f, 0);

	// draw skull shadow
	mSkull.mRasterizerState = nullptr;
	mSkull.mBlendState = mTransparentBS;
	mSkull.mDepthStencilState = mNoDoubleBlendDSS;
	mSkull.mStencilRef = 0;
	DrawGameObject(&mSkull);

	// restore skull world matrix and material
	mSkull.mWorld = CacheSkullWorld;
	mSkull.mMaterial = CacheSkullMaterial;

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}