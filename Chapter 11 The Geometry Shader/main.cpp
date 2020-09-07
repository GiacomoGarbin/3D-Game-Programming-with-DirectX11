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

	//ID3D11InputLayout* mInputLayout;

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

	GameObject mLand;
	GameObject mWaves;
	GameObject mBox;
	GameObject mTree;

	GeometryGenerator::Waves mWavesGenerator;

	std::vector<GameObject*> mObjects;

	LightDirectional mLights[3];

	ID3D11SamplerState* mSamplerState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mNoCullRS;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mCullClockwiseRS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mNoRenderTargetWritesBS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mTransparentBS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mAlphaToCoverageBS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mMarkMirrorDSS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDrawReflectionDSS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mNoDoubleBlendDSS;
};

TestApp::TestApp()
	: D3DApp()
{
	mMainWindowTitle = "Ch11 The Geometry Shader";

	m4xMSAAEnabled = true;

	mWorld = XMMatrixIdentity();
	mView = XMMatrixIdentity();
	mProj = XMMatrixIdentity();

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

	mLand.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLand.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mLand.mMaterial.mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);

	mWaves.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mWaves.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	mWaves.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 32.0f);

	mBox.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mBox.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mBox.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

	mTree.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mTree.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mTree.mMaterial.mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);
}

TestApp::~TestApp()
{
	//SafeRelease(mInputLayout);
	SafeRelease(mCbPerObject);
	SafeRelease(mCbPerFrame);
	SafeRelease(mSamplerState);
	SafeRelease((*mCullClockwiseRS.GetAddressOf()));
	SafeRelease((*mNoCullRS.GetAddressOf()));
	SafeRelease((*mNoRenderTargetWritesBS.GetAddressOf()));
	SafeRelease((*mTransparentBS.GetAddressOf()));
	SafeRelease((*mAlphaToCoverageBS.GetAddressOf()));
	SafeRelease((*mMarkMirrorDSS.GetAddressOf()));
	SafeRelease((*mDrawReflectionDSS.GetAddressOf()));
	SafeRelease((*mNoDoubleBlendDSS.GetAddressOf()));
}

bool TestApp::Init()
{
	if (D3DApp::Init())
	{
		std::wstring base = L"C:/Users/D3PO/source/repos/DirectX11Tutorial/Chapter 11 The Geometry Shader/";

		// vertex shader land and waves
		{
			Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;

			std::wstring path = base + L"VertexShader.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mLand.mVertexShader  = shader;
			mWaves.mVertexShader = shader;

			// input layout land and waves
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

				mLand.mInputLayout = layout;
				mWaves.mInputLayout = layout;
			} // input layout
		} // vertex shader

		// pixel shader land and waves
		{
			Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

			std::wstring path = base + L"PixelShader.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "USE_TEXTURE", "1" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mLand.mPixelShader  = shader;
			mWaves.mPixelShader = shader;
		} // pixel shader

		//// pixel shader no texture
		//{
		//	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

		//	std::vector<D3D_SHADER_MACRO> defines;
		//	defines.push_back({ "USE_TEXTURE", "0" });
		//	defines.push_back({ nullptr, nullptr });

		//	ID3DBlob* pCode;
		//	HR(D3DCompileFromFile(L"PixelShader.hlsl", defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		//	HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));
		//} // pixel shader

		// vertex shader tree
		{
			Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;

			std::wstring path = base + L"TreeVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mTree.mVertexShader = shader;

			// input layout tree
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"SIZE",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

				mTree.mInputLayout = layout;
			} // input layout
		} // vertex shader

		// geometry shader tree
		{
			Microsoft::WRL::ComPtr<ID3D11GeometryShader> shader;

			std::wstring path = base + L"TreeGS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "gs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateGeometryShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mTree.mGeometryShader = shader;
		} // geometry shader

		// pixel shader tree
		{
			Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

			std::wstring path = base + L"TreePS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "USE_TEXTURE", "1" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

			mTree.mPixelShader = shader;
		} // pixel shader

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


		// AlphaToCoverageBS
		{
			D3D11_BLEND_DESC desc;
			desc.AlphaToCoverageEnable = true;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			HR(mDevice->CreateBlendState(&desc, &mAlphaToCoverageBS));
		} // AlphaToCoverageBS

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

		// game objects
		{
			//// floor
			//{
			//	auto& v = mFloor.mMesh.mVertices;

			//	v.resize(1 * 2 * 3);

			//	v[0] = GeometryGenerator::Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f);
			//	v[1] = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
			//	v[2] = GeometryGenerator::Vertex(7.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f);

			//	v[3] = GeometryGenerator::Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f);
			//	v[4] = GeometryGenerator::Vertex(7.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f);
			//	v[5] = GeometryGenerator::Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f);

			//	mFloor.mVertexStart = 0;

			//	mFloor.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
			//	mFloor.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			//	mFloor.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

			//	mObjects.push_back(&mFloor);
			//}

			//// wall
			//{
			//	auto& v = mWall.mMesh.mVertices;

			//	v.resize(3 * 2 * 3);

			//	v[0] = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
			//	v[1] = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
			//	v[2] = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f);

			//	v[3] = GeometryGenerator::Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
			//	v[4] = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f);
			//	v[5] = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f);

			//	v[6] = GeometryGenerator::Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
			//	v[7] = GeometryGenerator::Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
			//	v[8] = GeometryGenerator::Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f);

			//	v[9] = GeometryGenerator::Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f);
			//	v[10] = GeometryGenerator::Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f);
			//	v[11] = GeometryGenerator::Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f);

			//	v[12] = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
			//	v[13] = GeometryGenerator::Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
			//	v[14] = GeometryGenerator::Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f);

			//	v[15] = GeometryGenerator::Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
			//	v[16] = GeometryGenerator::Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f);
			//	v[17] = GeometryGenerator::Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f);

			//	mWall.mVertexStart = mFloor.mVertexStart + mFloor.mMesh.mVertices.size();

			//	mWall.mMaterial.mAmbient = mFloor.mMaterial.mAmbient;
			//	mWall.mMaterial.mDiffuse = mFloor.mMaterial.mDiffuse;
			//	mWall.mMaterial.mSpecular = mFloor.mMaterial.mSpecular;

			//	mObjects.push_back(&mWall);
			//}

			//// mirror
			//{
			//	auto& v = mMirror.mMesh.mVertices;

			//	v.resize(1 * 2 * 3);

			//	v[0] = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
			//	v[1] = GeometryGenerator::Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
			//	v[2] = GeometryGenerator::Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);

			//	v[3] = GeometryGenerator::Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
			//	v[4] = GeometryGenerator::Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
			//	v[5] = GeometryGenerator::Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

			//	mMirror.mVertexStart = mWall.mVertexStart + mWall.mMesh.mVertices.size();

			//	mMirror.mMaterial.mAmbient = mFloor.mMaterial.mAmbient;
			//	mMirror.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
			//	mMirror.mMaterial.mSpecular = mFloor.mMaterial.mSpecular;

			//	mObjects.push_back(&mMirror);
			//}

			//// skull
			//{
			//	GeometryGenerator::CreateModel("skull.txt", mSkull.mMesh);

			//	mSkull.mVertexStart = 0;

			//	mSkull.mWorld = XMMatrixRotationY(XM_PIDIV2) * XMMatrixScaling(0.45f, 0.45f, 0.45f) * XMMatrixTranslation(0.0f, 1.0f, -5.0f);

			//	mSkull.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
			//	mSkull.mMaterial.mDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			//	mSkull.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

			//	mObjects.push_back(&mSkull);
			//}

			GeometryGenerator::CreateGrid(160, 160, 50, 50, mLand.mMesh);

			auto GetHeight = [](float x, float z) -> float
			{
				return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
			};

			for (GeometryGenerator::Vertex& v : mLand.mMesh.mVertices)
			{
				v.mPosition.y = GetHeight(v.mPosition.x, v.mPosition.z);
			}

			mLand.mMesh.ComputeNormals();

			mLand.mVertexStart = 0;

			mLand.mTexTransform = XMMatrixScaling(5, 5, 0);

			mObjects.push_back(&mLand);


			mTree.mMesh.mVertices.resize(16);

			for (auto& v : mTree.mMesh.mVertices)
			{
				float x = GameMath::RandNorm(-35.0f, 35.0f);
				float z = GameMath::RandNorm(-35.0f, 35.0f);
				float y = GetHeight(x, z) + 10;

				v.mPosition = XMFLOAT3(x, y, z);
				v.mTexCoord = XMFLOAT2(24, 24); // billboard size
			}

			mTree.mVertexStart = mLand.mVertexStart + mLand.mMesh.mVertices.size();

			mTree.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

			mTree.mBlendState = mAlphaToCoverageBS;

			mObjects.push_back(&mTree);


			GeometryGenerator::CreateGrid(160, 160, 160, 160, mWaves.mMesh);

			mWavesGenerator.Init(160, 160, 1, 0.03f, 5, 0.3f);

			mWaves.mBlendState = mTransparentBS;

			mObjects.push_back(&mWaves);

		} // game objects

		// vertex buffer land and tree
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * (mLand.mMesh.mVertices.size() + mTree.mMesh.mVertices.size());
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

			UpdateBuffer(mLand);
			UpdateBuffer(mTree);

			mLand.mVertexBuffer = buffer;
			mTree.mVertexBuffer = buffer;

		} // vertex buffer

		// index buffer land
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * mLand.mMesh.mIndices.size();
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			HR(mDevice->CreateBuffer(&desc, nullptr, &buffer));

			static auto UpdateBuffer = [this, &buffer](GameObject& obj) -> void
			{
				D3D11_BOX region;
				region.left = sizeof(UINT) * obj.mIndexStart;
				region.right = sizeof(UINT) * (obj.mIndexStart + obj.mMesh.mIndices.size());
				region.top = 0;
				region.bottom = 1;
				region.front = 0;
				region.back = 1;

				mContext->UpdateSubresource(*buffer.GetAddressOf(), 0, &region, obj.mMesh.mIndices.data(), 0, 0);
			};

			UpdateBuffer(mLand);

			mLand.mIndexBuffer = buffer;
		} // index buffer

		// vertex buffer waves
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mWaves.mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mWaves.mMesh.mVertices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mWaves.mVertexBuffer));
		} // vertex buffer

		// index buffer waves
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * mWaves.mMesh.mIndices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mWaves.mMesh.mIndices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mWaves.mIndexBuffer));
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
			mContext->GSSetConstantBuffers(0, 1, &mCbPerObject);
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

			mContext->GSSetConstantBuffers(1, 1, &mCbPerFrame);
			mContext->PSSetConstantBuffers(1, 1, &mCbPerFrame);
		} // constant buffer per frame

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

			static auto CreateTextureArray = [this](const std::vector<std::wstring>& names, ID3D11ShaderResourceView** view) -> void
			{
				std::wstring base = L"C:/Users/D3PO/source/repos/DirectX11Tutorial/textures/";

				std::vector<ID3D11Texture2D*> textures(names.size(), nullptr);

				for (UINT i = 0; i < names.size(); ++i)
				{
					std::wstring path = base + names.at(i);
					ID3D11Resource* resource = nullptr;

					HR(CreateDDSTextureFromFileEx(
						mDevice,
						path.c_str(),
						0,
						D3D11_USAGE_STAGING,
						0,
						D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,
						0,
						false,
						&resource,
						nullptr));

					HR(resource->QueryInterface(IID_ID3D11Texture2D, (void**)&textures.at(i)));
				}

				D3D11_TEXTURE2D_DESC TextureDesc;
				textures.front()->GetDesc(&TextureDesc);

				D3D11_TEXTURE2D_DESC TextureArrayDesc;
				TextureArrayDesc.Width = TextureDesc.Width;
				TextureArrayDesc.Height = TextureDesc.Height;
				TextureArrayDesc.MipLevels = TextureDesc.MipLevels;
				TextureArrayDesc.ArraySize = textures.size();
				TextureArrayDesc.Format = TextureDesc.Format;
				TextureArrayDesc.SampleDesc.Count = 1;
				TextureArrayDesc.SampleDesc.Quality = 0;
				TextureArrayDesc.Usage = D3D11_USAGE_DEFAULT;
				TextureArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				TextureArrayDesc.CPUAccessFlags = 0;
				TextureArrayDesc.MiscFlags = 0;

				ID3D11Texture2D* TextureArray = nullptr;
				HR(mDevice->CreateTexture2D(&TextureArrayDesc, nullptr, &TextureArray));

				for (UINT ArraySlice = 0; ArraySlice < textures.size(); ++ArraySlice)
				{
					ID3D11Texture2D* texture = textures.at(ArraySlice);

					for (UINT MipSlice = 0; MipSlice < TextureDesc.MipLevels; ++MipSlice)
					{
						D3D11_MAPPED_SUBRESOURCE MappedData;

						HR(mContext->Map(texture, MipSlice, D3D11_MAP_READ, 0, &MappedData));

						UINT subresource = D3D11CalcSubresource(MipSlice, ArraySlice, TextureDesc.MipLevels);
						mContext->UpdateSubresource(TextureArray, subresource, nullptr, MappedData.pData, MappedData.RowPitch, MappedData.DepthPitch);

						mContext->Unmap(texture, MipSlice);
					}
				}

				D3D11_SHADER_RESOURCE_VIEW_DESC desc;
				desc.Format = TextureArrayDesc.Format;
				desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				desc.Texture2DArray.MostDetailedMip = 0;
				desc.Texture2DArray.MipLevels = TextureArrayDesc.MipLevels;
				desc.Texture2DArray.FirstArraySlice = 0;
				desc.Texture2DArray.ArraySize = textures.size();

				HR(mDevice->CreateShaderResourceView(TextureArray, &desc, view));

				SafeRelease(TextureArray);

				for (ID3D11Texture2D* texture : textures)
				{
					SafeRelease(texture);
				}
			};

			CreateTexture(L"grass.dds", &mLand.mSRV);
			CreateTexture(L"water2.dds", &mWaves.mSRV);

			std::vector<std::wstring> names = { L"tree0.dds", L"tree1.dds", L"tree2.dds", L"tree3.dds" };
			CreateTextureArray(names, &mTree.mSRV);

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

	static float BaseTime = 0;

	if ((mTimer.TotalTime() - BaseTime) >= 0.25f)
	{
		BaseTime += 0.25f;

		UINT i = 5 + rand() % (mWavesGenerator.mRows - 10);
		UINT j = 5 + rand() % (mWavesGenerator.mCols - 10);

		float r = GameMath::RandNorm(0.5f, 1.0f);

		mWavesGenerator.Disturb(i, j, r);
	}

	mWavesGenerator.Update(dt);

	D3D11_MAPPED_SUBRESOURCE MappedData;

	HR(mContext->Map(mWaves.mVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData));

	GeometryGenerator::Vertex* vertex = reinterpret_cast<GeometryGenerator::Vertex*>(MappedData.pData);

	for (std::size_t i = 0; i < mWaves.mMesh.mVertices.size(); i++)
	{
		vertex[i] = mWavesGenerator[i];
	}

	mContext->Unmap(mWaves.mVertexBuffer.Get(), 0);

	auto GetHillHeight = [](float x, float z) -> float
	{
		return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
	};

	static XMFLOAT2 TexOffset = XMFLOAT2(0, 0);

	TexOffset.x += 0.10f * dt;
	TexOffset.y += 0.05f * dt;

	mWaves.mTexTransform = XMMatrixScaling(5, 5, 0) * XMMatrixTranslation(TexOffset.x, TexOffset.y, 0);
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

		// shaders
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
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

	for (GameObject* obj : mObjects)
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