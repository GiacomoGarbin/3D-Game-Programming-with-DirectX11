#include "D3DApp.h"

#include <sstream>
#include <cassert>
#include <vector>
//#include <cstdlib>

#include <directxpackedvector.h>

GameTimer::GameTimer() :
	mSecondsPerCount(0),
	mDeltaTime(-1),
	mBaseTime(0),
	mPauseTime(0),
	mPrevTime(0),
	mCurrTime(0),
	mStopped(false)
{
	__int64 freq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	mSecondsPerCount = 1.0 / (double)freq;
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
	mPrevTime = mCurrTime;

	if (mDeltaTime < 0)
	{
		mDeltaTime = 0;
	}
}

float GameTimer::DeltaTime() const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Start()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	if (mStopped)
	{
		mPauseTime += (currTime - mStopTime);
		mPrevTime = currTime;
		mStopTime = 0;
		mStopped = false;
	}
}

float GameTimer::TotalTime() const
{
	return (float)((((mStopped ? mStopTime : mCurrTime) - mPauseTime) - mBaseTime) * mSecondsPerCount);
}

D3DApp::D3DApp() :
	mAppPaused(false),
	mResizing(false),
	m4xMSAAEnabled(false),
	m4xMSAAQuality(0),

	mDevice(nullptr),
	mContext(nullptr),
	mSwapChain(nullptr),
	mDepthStencilBuffer(nullptr),
	mRenderTargetView(nullptr),
	mDepthStencilView(nullptr),

	mMainWindow(nullptr),
	mMainWindowWidth(800),
	mMainWindowHeight(600),
	mMainWindowTitle("D3DApp"),

	mTheta(1.24f * XM_PI),
	mPhi(0.42f * XM_PI),
	mRadius(50),
	mLastMousePos(0, 0)
{
	ZeroMemory(&mViewport, sizeof(D3D11_VIEWPORT));

	std::fill(mKeysState.begin(), mKeysState.end(), false);
	
	mCamera.mPosition = XMFLOAT3(0, 2, -15);
	// mCamera.mPosition = XMVectorSet(0, 2, -15, 1);
}

D3DApp::~D3DApp()
{
	SafeRelease(mRenderTargetView);
	SafeRelease(mDepthStencilView);
	SafeRelease(mDepthStencilBuffer);
	SafeRelease(mSwapChain);
	if (mContext) mContext->ClearState();
	SafeRelease(mContext);
	SafeRelease(mDevice);

	SafeRelease((*mWireframeRS.GetAddressOf()));
	SafeRelease((*mNoCullRS.GetAddressOf()));
	
	SafeRelease((*mLessEqualDSS.GetAddressOf()));
	SafeRelease((*mEqualDSS.GetAddressOf()));

	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}

int D3DApp::Run()
{
	mTimer.Reset();

	while (!glfwWindowShouldClose(mMainWindow))
	{
		glfwPollEvents();

		mTimer.Tick();

		if (!mAppPaused)
		{
			CalculateFrameStats();
			UpdateScene(mTimer.DeltaTime());
			DrawScene();
		}
		else
		{
			Sleep(100);
		}
	}

	return 0;
}

bool D3DApp::Init()
{
	if (!InitMainWindow()) return false;
	if (!InitDirect3D()) return false;

	mTextureManager.Init(mDevice, mContext);

	return true;
}

bool D3DApp::InitMainWindow()
{
	if (glfwInit() == GLFW_FALSE) return false;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mMainWindow = glfwCreateWindow(mMainWindowWidth, mMainWindowHeight, mMainWindowTitle.c_str(), nullptr, nullptr);

	if (!mMainWindow) return false;

	glfwSetWindowUserPointer(mMainWindow, this);

	glfwSetWindowSizeCallback(mMainWindow, [](GLFWwindow* window, int width, int height)
	{
		D3DApp* app = reinterpret_cast<D3DApp*>(glfwGetWindowUserPointer(window));
		app->OnResize(window, width, height);
	});

	glfwSetMouseButtonCallback(mMainWindow, [](GLFWwindow* window, int button, int action, int mods)
	{
		D3DApp* app = reinterpret_cast<D3DApp*>(glfwGetWindowUserPointer(window));
		app->OnMouseButton(window, button, action, mods);
	});

	glfwSetCursorPosCallback(mMainWindow, [](GLFWwindow* window, double xpos, double ypos)
	{
		D3DApp* app = reinterpret_cast<D3DApp*>(glfwGetWindowUserPointer(window));
		app->OnMouseMove(window, xpos, ypos);
	});

	glfwSetKeyCallback(mMainWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		D3DApp* app = reinterpret_cast<D3DApp*>(glfwGetWindowUserPointer(window));
		app->OnKeyButton(window, key, scancode, action, mods);
	});

	return true;
}

bool D3DApp::InitDirect3D()
{
	IDXGIFactory1* pFactory = nullptr;
	HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory));

	std::vector<IDXGIAdapter1*> vAdapters;
	IDXGIAdapter1* pAdapter;

	for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		vAdapters.push_back(pAdapter);
	}

	SafeRelease(pFactory);

	for (IDXGIAdapter1* pAdapter : vAdapters)
	{
		DXGI_ADAPTER_DESC desc;
		HR(pAdapter->GetDesc(&desc));
		//std::wcout << desc.Description << std::endl;
	}

	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	D3D_FEATURE_LEVEL FeatureLevels;

	HR(D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		flags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&mDevice,
		&FeatureLevels,
		&mContext
	));

	if (FeatureLevels < D3D_FEATURE_LEVEL_11_0)
	{
		std::cout << "featureLevel < D3D_FEATURE_LEVEL_11_0" << std::endl;
		return false;
	}

	HR(mDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m4xMSAAQuality));
	assert(m4xMSAAQuality > 0);

	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferDesc.Width = mMainWindowWidth;
	desc.BufferDesc.Height = mMainWindowHeight;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	if (m4xMSAAEnabled)
	{
		desc.SampleDesc.Count = 4;
		desc.SampleDesc.Quality = m4xMSAAQuality - 1;
	}
	else
	{
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
	}

	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.OutputWindow = glfwGetWin32Window(mMainWindow);
	desc.Windowed = true;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // DXGI_SWAP_EFFECT_FLIP_DISCARD
	desc.Flags = 0;

	IDXGIDevice* device = nullptr;
	HR(mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&device));

	IDXGIAdapter* adapter = nullptr;
	HR(device->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter));

	IDXGIFactory* factory = nullptr;
	HR(adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory));

	HR(factory->CreateSwapChain(mDevice, &desc, &mSwapChain));

	SafeRelease(device);
	SafeRelease(adapter);
	SafeRelease(factory);

	OnResize(mMainWindow, mMainWindowWidth, mMainWindowHeight);

	// rasterizer states

	// wireframe
	{
		D3D11_RASTERIZER_DESC desc;
		desc.FillMode = D3D11_FILL_WIREFRAME;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = false;
		desc.DepthBias = 0;
		desc.DepthBiasClamp = 0;
		desc.SlopeScaledDepthBias = 0;
		desc.DepthClipEnable = true;
		desc.ScissorEnable = false;
		desc.MultisampleEnable = false;
		desc.AntialiasedLineEnable = false;

		HR(mDevice->CreateRasterizerState(&desc, &mWireframeRS));
	}

	// no cull
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
	}

	// depth stencil states

	// less equal
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = false;
		//desc.StencilReadMask = 0xff;
		//desc.StencilWriteMask = 0xff;
		//desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		//desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		//desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		//desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		HR(mDevice->CreateDepthStencilState(&desc, &mLessEqualDSS));
	}

	// equal
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_EQUAL;
		desc.StencilEnable = false;
		//desc.StencilReadMask = 0xff;
		//desc.StencilWriteMask = 0xff;
		//desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		//desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		//desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		//desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		//desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		HR(mDevice->CreateDepthStencilState(&desc, &mEqualDSS));
	}

	return true;
}

void D3DApp::OnResize(GLFWwindow* window, int width, int height)
{
	mMainWindowWidth = width;
	mMainWindowHeight = height;

	assert(mDevice);
	assert(mContext);
	assert(mSwapChain);

	SafeRelease(mRenderTargetView);
	SafeRelease(mDepthStencilView);
	SafeRelease(mDepthStencilBuffer);

	// resize the swap chain and create the render target view

	HR(mSwapChain->ResizeBuffers(
		1,
		mMainWindowWidth,
		mMainWindowHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0
	));

	ID3D11Texture2D* backBuffer = nullptr;
	HR(mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)));
	HR(mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTargetView));
	SafeRelease(backBuffer);

	// create the depth-stencil buffer and view

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = mMainWindowWidth;
	desc.Height = mMainWindowHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (m4xMSAAEnabled)
	{
		desc.SampleDesc.Count = 4;
		desc.SampleDesc.Quality = m4xMSAAQuality - 1;
	}
	else
	{
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
	}

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	HR(mDevice->CreateTexture2D(&desc, 0, &mDepthStencilBuffer));
	HR(mDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));

	// bind the render target and depth-stencil views to the pipeline

	mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	// set the viewport

	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = mMainWindowWidth;
	mViewport.Height = mMainWindowHeight;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;

	mContext->RSSetViewports(1, &mViewport);

	// update proj matrix

	mCamera.SetFrustum(XM_PIDIV4, AspectRatio(), 1, 1000);
}

void D3DApp::OnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		mLastMousePos = XMFLOAT2(x, y);
	}
}

void D3DApp::OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
	//if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	//{
	//	float dx = 0.25f * XMConvertToRadians(xpos - mLastMousePos.x);
	//	float dy = 0.25f * XMConvertToRadians(ypos - mLastMousePos.y);

	//	mTheta += dx;
	//	mPhi += dy;

	//	mPhi = std::clamp(mPhi, 0.1f, XM_PI - 0.1f);
	//}
	//else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
	//{
	//	float dx = 0.5f * XMConvertToRadians(xpos - mLastMousePos.x);
	//	float dy = 0.5f * XMConvertToRadians(ypos - mLastMousePos.y);

	//	mRadius += (dx - dy);

	//	mRadius = std::clamp(mRadius, 3.f, 150.f);
	//}


	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		float dx = XMConvertToRadians(0.25f * (xpos - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * (ypos - mLastMousePos.y));

		mCamera.pitch(dy);
		mCamera.rotate(dx);

		mLastMousePos = XMFLOAT2(xpos, ypos);
	}
}

void D3DApp::OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_UNKNOWN) return;

	switch (action)
	{
		case GLFW_PRESS:
			mKeysState[key] = true;
			break;
		case GLFW_RELEASE:
			mKeysState[key] = false;
			break;
		case GLFW_REPEAT:
			break;
	}
}

void D3DApp::CalculateFrameStats()
{
	static std::size_t FrameCount = 0;
	static float TimeElapsed = 0;

	FrameCount++;

	if ((mTimer.TotalTime() - TimeElapsed) >= 1)
	{
		float fps = (float)FrameCount; // fps = frameCount / 1 second
		float FrameTime = 1000 / fps; // in ms

		std::ostringstream oss;
		oss << mMainWindowTitle << " | WR: " << mMainWindowWidth << "x" << mMainWindowHeight << " | FPS: " << fps << " | FT: " << FrameTime << " (ms)";
		glfwSetWindowTitle(mMainWindow, oss.str().c_str());

		FrameCount = 0;
		TimeElapsed += 1;
	}
}

float D3DApp::AspectRatio() const
{
	return static_cast<float>(mMainWindowWidth) / static_cast<float>(mMainWindowHeight);
}

void GeometryGenerator::CreateBox(float width, float height, float depth, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	//		  v1---------v2		v0 = black
	//		 /|         /|		v1 = blue
	//		v5---------v6|		v2 = green
	//		| |        | |		v3 = cyan
	//		| |        | |		v4 = red
	//		| v0-------|-v3		v5 = magenta
	//		|/         |/ 		v6 = yellow
	//		v4---------v7 		v7 = white

	//mesh.mVertices =
	//{
	//	{XMFLOAT3(-1, -1, -1), XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(-1, +1, -1), XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 1, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(+1, +1, -1), XMFLOAT3(0, 0, 0), XMFLOAT4(0, 1, 0, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(+1, -1, -1), XMFLOAT3(0, 0, 0), XMFLOAT4(0, 1, 1, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(-1, -1, +1), XMFLOAT3(0, 0, 0), XMFLOAT4(1, 0, 0, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(-1, +1, +1), XMFLOAT3(0, 0, 0), XMFLOAT4(1, 0, 1, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(+1, +1, +1), XMFLOAT3(0, 0, 0), XMFLOAT4(1, 1, 0, 1), XMFLOAT2(0, 0)},
	//	{XMFLOAT3(+1, -1, +1), XMFLOAT3(0, 0, 0), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0, 0)},
	//};

	//mesh.mIndices =
	//{
	//	0, 1, 2,   0, 2, 3,		// front
	//	4, 6, 5,   4, 7, 6,		// back
	//	4, 5, 1,   4, 1, 0,		// left
	//	3, 2, 6,   3, 6, 7,		// right
	//	1, 5, 6,   1, 6, 2,		// top
	//	4, 0, 3,   4, 3, 7,		// bottom
	//};

	//mesh.ComputeNormals();

	//for (Vertex& vertex : mesh.mVertices)
	//{
	//	vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);
	//}


	float w = 0.5f * width;
	float h = 0.5f * height;
	float d = 0.5f * depth;

	mesh.mVertices.resize(24);

	// front
	mesh.mVertices.at(0x00) = Vertex(-w, -h, -d, 0, 0, -1, +1, 0, 0, 0, 1);
	mesh.mVertices.at(0x01) = Vertex(-w, +h, -d, 0, 0, -1, +1, 0, 0, 0, 0);
	mesh.mVertices.at(0x02) = Vertex(+w, +h, -d, 0, 0, -1, +1, 0, 0, 1, 0);
	mesh.mVertices.at(0x03) = Vertex(+w, -h, -d, 0, 0, -1, +1, 0, 0, 1, 1);

	// back
	mesh.mVertices.at(0x04) = Vertex(-w, -h, +d, 0, 0, +1, -1, 0, 0, 1, 1);
	mesh.mVertices.at(0x05) = Vertex(+w, -h, +d, 0, 0, +1, -1, 0, 0, 0, 1);
	mesh.mVertices.at(0x06) = Vertex(+w, +h, +d, 0, 0, +1, -1, 0, 0, 0, 0);
	mesh.mVertices.at(0x07) = Vertex(-w, +h, +d, 0, 0, +1, -1, 0, 0, 1, 0);

	// top
	mesh.mVertices.at(0x08) = Vertex(-w, +h, -d, 0, +1, 0, +1, 0, 0, 0, 1);
	mesh.mVertices.at(0x09) = Vertex(-w, +h, +d, 0, +1, 0, +1, 0, 0, 0, 0);
	mesh.mVertices.at(0x0A) = Vertex(+w, +h, +d, 0, +1, 0, +1, 0, 0, 1, 0);
	mesh.mVertices.at(0x0B) = Vertex(+w, +h, -d, 0, +1, 0, +1, 0, 0, 1, 1);

	// bottom
	mesh.mVertices.at(0x0C) = Vertex(-w, -h, -d, 0, -1, 0, -1, 0, 0, 1, 1);
	mesh.mVertices.at(0x0D) = Vertex(+w, -h, -d, 0, -1, 0, -1, 0, 0, 0, 1);
	mesh.mVertices.at(0x0E) = Vertex(+w, -h, +d, 0, -1, 0, -1, 0, 0, 0, 0);
	mesh.mVertices.at(0x0F) = Vertex(-w, -h, +d, 0, -1, 0, -1, 0, 0, 1, 0);

	// left
	mesh.mVertices.at(0x10) = Vertex(-w, -h, +d, -1, 0, 0, 0, 0, -1, 0, 1);
	mesh.mVertices.at(0x11) = Vertex(-w, +h, +d, -1, 0, 0, 0, 0, -1, 0, 0);
	mesh.mVertices.at(0x12) = Vertex(-w, +h, -d, -1, 0, 0, 0, 0, -1, 1, 0);
	mesh.mVertices.at(0x13) = Vertex(-w, -h, -d, -1, 0, 0, 0, 0, -1, 1, 1);

	// right
	mesh.mVertices.at(0x14) = Vertex(+w, -h, -d, +1, 0, 0, 0, 0, +1, 0, 1);
	mesh.mVertices.at(0x15) = Vertex(+w, +h, -d, +1, 0, 0, 0, 0, +1, 0, 0);
	mesh.mVertices.at(0x16) = Vertex(+w, +h, +d, +1, 0, 0, 0, 0, +1, 1, 0);
	mesh.mVertices.at(0x17) = Vertex(+w, -h, +d, +1, 0, 0, 0, 0, +1, 1, 1);

	mesh.mIndices.resize(36);
	std::vector<UINT>& i = mesh.mIndices;

	// front
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// back
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// top
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// bottom
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// left
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// right
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;
}

void GeometryGenerator::CreateGrid(float width, float depth, UINT m, UINT n, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	UINT VertexCount = m * n;
	UINT FaceCount = (m - 1) * (n - 1) * 2;

	float HalfWidth = width * 0.5f;
	float HalfDepth = depth * 0.5f;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	mesh.mVertices.resize(VertexCount);

	for (UINT i = 0; i < m; i++)
	{
		float z = HalfDepth - i * dz;

		for (UINT j = 0; j < n; j++)
		{
			float x = -HalfWidth + j * dx;

			std::size_t k = i * n + j;

			mesh.mVertices.at(k).mPosition = XMFLOAT3(x, 0, z);
			mesh.mVertices.at(k).mNormal   = XMFLOAT3(0, 1, 0);
			mesh.mVertices.at(k).mTangent  = XMFLOAT3(1, 0, 0);
			//mesh.mVertices.at(k).mColor    = XMFLOAT4((x + HalfWidth) / width, 0, (z + HalfDepth) / depth, 1);
			mesh.mVertices.at(k).mTexCoord = XMFLOAT2(j * du, i * dv);
		}
	}

	mesh.mIndices.resize(FaceCount * 3);

	for (UINT i = 0; i < m - 1; i++)
	{
		for (UINT j = 0; j < n - 1; j++)
		{
			std::size_t k = i * (n - 1) + j;

			mesh.mIndices.at(k * 6 + 0) = (i + 0) * n + (j + 0);
			mesh.mIndices.at(k * 6 + 1) = (i + 0) * n + (j + 1);
			mesh.mIndices.at(k * 6 + 2) = (i + 1) * n + (j + 0);
			mesh.mIndices.at(k * 6 + 3) = (i + 1) * n + (j + 0);
			mesh.mIndices.at(k * 6 + 4) = (i + 0) * n + (j + 1);
			mesh.mIndices.at(k * 6 + 5) = (i + 1) * n + (j + 1);
		}
	}
}

void GeometryGenerator::CreateCylinder(float BottomRadius, float TopRadius, float height, UINT slices, UINT stacks, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	float dy = height / stacks; // stack height
	float dr = (TopRadius - BottomRadius) / stacks; // radius step

	UINT rings = stacks + 1;

	for (UINT i = 0; i < rings; i++)
	{
		float y = -0.5f * height + i * dy;
		float r = BottomRadius + i * dr;

		float dt = XM_2PI / slices; // delta theta

		for (UINT j = 0; j <= slices; j++)
		{
			Vertex vertex;

			float c = std::cos(j * dt);
			float s = std::sin(j * dt);

			vertex.mPosition = XMFLOAT3(r * c, y, r * s);
			vertex.mTexCoord = XMFLOAT2((float)j / slices, 1 - (float)i / stacks);

			XMVECTOR T = XMVectorSet(-s, 0, c, 0);
			XMVECTOR B = XMVectorSet((dr * stacks) * c, -height, (dr * stacks) * s, 0);
			XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));

			XMStoreFloat3(&vertex.mNormal, N);
			XMStoreFloat3(&vertex.mTangent, T);

			//vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

			mesh.mVertices.push_back(vertex);
		}
	}

	UINT vertices = slices + 1; // vertex per ring

	for (UINT i = 0; i < stacks; i++)
	{
		for (UINT j = 0; j < slices; j++)
		{
			mesh.mIndices.push_back((i + 0) * vertices + (j + 0));
			mesh.mIndices.push_back((i + 1) * vertices + (j + 0));
			mesh.mIndices.push_back((i + 1) * vertices + (j + 1));

			mesh.mIndices.push_back((i + 0) * vertices + (j + 0));
			mesh.mIndices.push_back((i + 1) * vertices + (j + 1));
			mesh.mIndices.push_back((i + 0) * vertices + (j + 1));
		}
	}

	enum CapType
	{
		TopCap = 0,
		BottomCap = 1
	};

	auto BuildCap = [&](CapType type)
	{
		UINT BaseIndex = mesh.mVertices.size();

		float y = (type ? +1 : -1) * 0.5f * height;

		float radius = type ? TopRadius : BottomRadius;

		float dt = XM_2PI / slices; // delta theta

		for (UINT j = 0; j <= slices; j++)
		{
			float x = radius * std::cos(j * dt);
			float z = radius * std::sin(j * dt);

			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			Vertex vertex;
			vertex.mPosition = XMFLOAT3(x, y, z);
			vertex.mNormal = XMFLOAT3(0, type ? +1 : -1, 0);
			vertex.mTangent = XMFLOAT3(1, 0, 0);
			//vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);
			vertex.mTexCoord = XMFLOAT2(u, v);

			mesh.mVertices.push_back(vertex);
		}

		Vertex vertex;
		vertex.mPosition = XMFLOAT3(0, y, 0);
		vertex.mNormal = XMFLOAT3(0, type ? +1 : -1, 0);
		vertex.mTangent = XMFLOAT3(1, 0, 0);
		//vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);
		vertex.mTexCoord = XMFLOAT2(0.5f, 0.5f);

		mesh.mVertices.push_back(vertex);

		UINT CenterIndex = mesh.mVertices.size() - 1;

		for (UINT j = 0; j < slices; j++)
		{
			mesh.mIndices.push_back(CenterIndex);
			mesh.mIndices.push_back(BaseIndex + j + (type ? 1 : 0));
			mesh.mIndices.push_back(BaseIndex + j + (type ? 0 : 1));
		}
	};

	BuildCap(TopCap);
	BuildCap(BottomCap);
}

void GeometryGenerator::CreateSphere(float radius, UINT n, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	const float x = 0.525731f;
	const float z = 0.850651f;

	mesh.mVertices.resize(12);
	mesh.mVertices.at(0x0).mPosition = XMFLOAT3(-x, 0, +z);
	mesh.mVertices.at(0x1).mPosition = XMFLOAT3(+x, 0, +z);
	mesh.mVertices.at(0x2).mPosition = XMFLOAT3(-x, 0, -z);
	mesh.mVertices.at(0x3).mPosition = XMFLOAT3(+x, 0, -z);
	mesh.mVertices.at(0x4).mPosition = XMFLOAT3(0, +z, +x);
	mesh.mVertices.at(0x5).mPosition = XMFLOAT3(0, +z, -x);
	mesh.mVertices.at(0x6).mPosition = XMFLOAT3(0, -z, +x);
	mesh.mVertices.at(0x7).mPosition = XMFLOAT3(0, -z, -x);
	mesh.mVertices.at(0x8).mPosition = XMFLOAT3(+z, +x, 0);
	mesh.mVertices.at(0x9).mPosition = XMFLOAT3(-z, +x, 0);
	mesh.mVertices.at(0xA).mPosition = XMFLOAT3(+z, -x, 0);
	mesh.mVertices.at(0xB).mPosition = XMFLOAT3(-z, -x, 0);

	mesh.mIndices =
	{
		0x1, 0x4, 0x0,
		0x4, 0x9, 0x0,
		0x4, 0x5, 0x9,
		0x8, 0x5, 0x4,
		0x1, 0x8, 0x4,

		0x1, 0xA, 0x8,
		0xA, 0x3, 0x8,
		0x8, 0x3, 0x5,
		0x3, 0x2, 0x5,
		0x3, 0x7, 0x2,

		0x3, 0xA, 0x7,
		0xA, 0x6, 0x7,
		0x6, 0xB, 0x7,
		0x6, 0x0, 0xB,
		0x6, 0x1, 0x0,

		0xA, 0x1, 0x6,
		0xB, 0x0, 0x9,
		0x2, 0xB, 0x9,
		0x5, 0x2, 0x9,
		0xB, 0x2, 0x7,
	};

	auto subdivide = [&]()
	{
		Mesh copy = mesh;

		mesh.mVertices.clear();
		mesh.mIndices.clear();

		for (UINT i = 0; i < (copy.mIndices.size() / 3); i++)
		{
			Vertex v0 = copy.mVertices.at(copy.mIndices.at(i * 3 + 0));
			Vertex v1 = copy.mVertices.at(copy.mIndices.at(i * 3 + 1));
			Vertex v2 = copy.mVertices.at(copy.mIndices.at(i * 3 + 2));

			Vertex m0, m1, m2;
			m0.mPosition = XMFLOAT3(0.5f * (v0.mPosition.x + v1.mPosition.x), 0.5f * (v0.mPosition.y + v1.mPosition.y), 0.5f * (v0.mPosition.z + v1.mPosition.z));
			m1.mPosition = XMFLOAT3(0.5f * (v1.mPosition.x + v2.mPosition.x), 0.5f * (v1.mPosition.y + v2.mPosition.y), 0.5f * (v1.mPosition.z + v2.mPosition.z));
			m2.mPosition = XMFLOAT3(0.5f * (v2.mPosition.x + v0.mPosition.x), 0.5f * (v2.mPosition.y + v0.mPosition.y), 0.5f * (v2.mPosition.z + v0.mPosition.z));

			mesh.mVertices.push_back(v0);
			mesh.mVertices.push_back(v1);
			mesh.mVertices.push_back(v2);
			mesh.mVertices.push_back(m0);
			mesh.mVertices.push_back(m1);
			mesh.mVertices.push_back(m2);

			mesh.mIndices.push_back(i * 6 + 0);
			mesh.mIndices.push_back(i * 6 + 3);
			mesh.mIndices.push_back(i * 6 + 5);

			mesh.mIndices.push_back(i * 6 + 3);
			mesh.mIndices.push_back(i * 6 + 4);
			mesh.mIndices.push_back(i * 6 + 5);

			mesh.mIndices.push_back(i * 6 + 5);
			mesh.mIndices.push_back(i * 6 + 4);
			mesh.mIndices.push_back(i * 6 + 2);

			mesh.mIndices.push_back(i * 6 + 3);
			mesh.mIndices.push_back(i * 6 + 1);
			mesh.mIndices.push_back(i * 6 + 4);
		}
	};

	while (n--)
	{
		subdivide();
	}

	for (Vertex& vertex : mesh.mVertices)
	{
		XMVECTOR N = XMVector3Normalize(XMLoadFloat3(&vertex.mPosition));
		XMVECTOR P = radius * N;

		XMStoreFloat3(&vertex.mPosition, P);
		XMStoreFloat3(&vertex.mNormal, N);

		//vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

		// frank luna
		{
			// theta & phi
			float t = GameMath::GetAngle2(XMFLOAT2(vertex.mPosition.x, vertex.mPosition.z));
			float p = std::acos(vertex.mPosition.y / radius);

			// texture coordinates
			vertex.mTexCoord.x = t / XM_2PI;
			vertex.mTexCoord.y = p / XM_PI;

			// tangent
			vertex.mTangent.x = -radius * std::sin(p) * std::sin(t);
			vertex.mTangent.y = 0;
			vertex.mTangent.z = +radius * std::sin(p) * std::cos(t);

			XMVECTOR T = XMLoadFloat3(&vertex.mTangent);
			XMStoreFloat3(&vertex.mTangent, XMVector3Normalize(T));
		}

		// wikipedia : texture mapping
		{
			XMVECTOR D = N;
			XMFLOAT3 d;
			XMStoreFloat3(&d, D);

			vertex.mTexCoord.x = 0.5f + std::atan2(d.x, d.z) / XM_2PI;
			vertex.mTexCoord.y = 0.5f - std::asin(d.y) / XM_PI;
		}
	}
}

void GeometryGenerator::CreateSphere(float radius, UINT slices, UINT stacks, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();
}

void GeometryGenerator::CreateModel(std::string name, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	std::ifstream ifs;

	//ifs.open("C:/Users/ggarbin/Desktop/3D-Game-Programming-with-DirectX11/models/" + name);
	ifs.open("C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/models/" + name);

	std::string line;

	bool vertices = false;
	bool indices = false;

	XMFLOAT3 minima = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxima = XMFLOAT3(FLT_MIN, FLT_MIN, FLT_MIN);
	XMVECTOR min = XMLoadFloat3(&minima);
	XMVECTOR max = XMLoadFloat3(&maxima);

	while (std::getline(ifs, line))
	{
		if (vertices)
		{
			std::stringstream ss(line);
			GeometryGenerator::Vertex vertex;

			ss >> vertex.mPosition.x >> vertex.mPosition.y >> vertex.mPosition.z;
			ss >> vertex.mNormal.x >> vertex.mNormal.y >> vertex.mNormal.z;

			//vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

			mesh.mVertices.push_back(vertex);

			XMVECTOR P = XMLoadFloat3(&vertex.mPosition);
			min = XMVectorMin(min, P);
			max = XMVectorMax(max, P);
		}

		if (indices)
		{
			std::stringstream ss(line);

			UINT a, b, c;
			ss >> a >> b >> c;

			mesh.mIndices.push_back(a);
			mesh.mIndices.push_back(b);
			mesh.mIndices.push_back(c);
		}

		if (line == "{")
		{
			vertices = !vertices;
			indices = !vertices;
		}
	}

	ifs.close();

	XMStoreFloat3(&mesh.mAABB.Center,  (min + max) * 0.5f);
	XMStoreFloat3(&mesh.mAABB.Extents, (max - min) * 0.5f);

	// mesh.ComputeNormals();
}

void GeometryGenerator::CreateScreenQuad(Mesh& mesh)
{
	auto& v = mesh.mVertices;
	auto& i = mesh.mIndices;

	v.clear();
	i.clear();

	v.resize(4);

	v.at(0) = Vertex(-1, -1, 0,   0, 0, -1,   1, 0, 0,   0, 1);
	v.at(1) = Vertex(-1, +1, 0,   0, 0, -1,   1, 0, 0,   0, 0);
	v.at(2) = Vertex(+1, +1, 0,   0, 0, -1,   1, 0, 0,   1, 0);
	v.at(3) = Vertex(+1, -1, 0,   0, 0, -1,   1, 0, 0,   1, 1);

	i.resize(6);

	i.at(0) = 0;   i.at(1) = 1;   i.at(2) = 2;
	i.at(3) = 0;   i.at(4) = 2;   i.at(5) = 3;
}

//void GeometryGenerator::Mesh::ComputeNormals(std::vector<Vertex>& vertices, std::vector<UINT>& indices)
//{
//	for (Vertex& vertex : vertices)
//	{
//		vertex.mNormal = XMFLOAT3(0, 0, 0);
//	}
//
//	for (UINT j = 0; j < indices.size(); j += 3)
//	{
//		UINT i0 = indices.at(j + 0);
//		UINT i1 = indices.at(j + 1);
//		UINT i2 = indices.at(j + 2);
//
//		Vertex& v0 = vertices.at(i0);
//		Vertex& v1 = vertices.at(i1);
//		Vertex& v2 = vertices.at(i2);
//
//		XMVECTOR p0 = XMLoadFloat3(&v0.mPosition);
//		XMVECTOR p1 = XMLoadFloat3(&v1.mPosition);
//		XMVECTOR p2 = XMLoadFloat3(&v2.mPosition);
//
//		XMVECTOR a = p1 - p0;
//		XMVECTOR b = p2 - p0;
//		XMVECTOR N = XMVector3Cross(a, b);
//
//		XMStoreFloat3(&v0.mNormal, XMLoadFloat3(&v0.mNormal) + N);;
//		XMStoreFloat3(&v1.mNormal, XMLoadFloat3(&v1.mNormal) + N);;
//		XMStoreFloat3(&v2.mNormal, XMLoadFloat3(&v2.mNormal) + N);;
//	}
//
//	for (Vertex& vertex : vertices)
//	{
//		XMStoreFloat3(&vertex.mNormal, XMVector3Normalize(XMLoadFloat3(&vertex.mNormal)));
//	}
//}

void GeometryGenerator::Mesh::ComputeNormals(
	std::vector<Vertex>::iterator FirstVertex, std::vector<Vertex>::iterator LastVertex,
	std::vector<UINT>::iterator FirstIndex, std::vector<UINT>::iterator LastIndex)
{
	for (std::vector<Vertex>::iterator vertex = FirstVertex; vertex != LastVertex; vertex++)
	{
		vertex->mNormal = XMFLOAT3(0, 0, 0);
	}

	for (std::vector<UINT>::iterator index = FirstIndex; index != LastIndex; index += 3)
	{
		UINT i0 = *(index + 0);
		UINT i1 = *(index + 1);
		UINT i2 = *(index + 2);
		
		Vertex& v0 = *(FirstVertex + i0);
		Vertex& v1 = *(FirstVertex + i1);
		Vertex& v2 = *(FirstVertex + i2);

		XMVECTOR p0 = XMLoadFloat3(&v0.mPosition);
		XMVECTOR p1 = XMLoadFloat3(&v1.mPosition);
		XMVECTOR p2 = XMLoadFloat3(&v2.mPosition);

		XMVECTOR a = p1 - p0;
		XMVECTOR b = p2 - p0;
		XMVECTOR N = XMVector3Cross(a, b);

		XMStoreFloat3(&v0.mNormal, XMLoadFloat3(&v0.mNormal) + N);;
		XMStoreFloat3(&v1.mNormal, XMLoadFloat3(&v1.mNormal) + N);;
		XMStoreFloat3(&v2.mNormal, XMLoadFloat3(&v2.mNormal) + N);;
	}

	for (std::vector<Vertex>::iterator vertex = FirstVertex; vertex != LastVertex; vertex++)
	{
		XMStoreFloat3(&vertex->mNormal, XMVector3Normalize(XMLoadFloat3(&vertex->mNormal)));
	}
}

GeometryGenerator::Waves::Waves() :
	mRows(0),
	mCols(0),
	mK1(0),
	mK2(0),
	mK3(0),
	mTimeStep(0),
	mSpaceStep(0)
{}

GeometryGenerator::Waves::~Waves()
{
	mPrevVertices.clear();
	mCurrVertices.clear();
}

void GeometryGenerator::Waves::Init(UINT m, UINT n, float dx, float dt, float speed, float damping)
{
	mRows = m;
	mCols = n;

	mTimeStep = dt;
	mSpaceStep = dx;

	float d = damping * dt + 2;
	float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK1 = (damping * dt - 2) / d;
	mK2 = (4 - 8 * e) / d;
	mK3 = (2 * e) / d;

	mPrevVertices.clear();
	mCurrVertices.clear();

	const UINT vertices = m * n;
	const UINT triangles = (m - 1) * (n - 1) * 2;

	mPrevVertices.resize(vertices);
	mCurrVertices.resize(vertices);

	float HalfWidth = (n - 1) * dx * 0.5f;
	float HalfDepth = (m - 1) * dx * 0.5f;

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	for (UINT i = 0; i < m; i++)
	{
		float z = HalfDepth - i * dx;

		for (UINT j = 0; j < n; j++)
		{
			float x =  j * dx - HalfWidth;

			Vertex vertex;
			vertex.mPosition = XMFLOAT3(x, 0, z);
			vertex.mNormal = XMFLOAT3(0, 1, 0);
			//vertex.mColor = XMFLOAT4(0, 1, 1, 1);
			vertex.mTexCoord = XMFLOAT2(j * du, i * dv);

			mPrevVertices.at(i * n + j) = vertex;
			mCurrVertices.at(i * n + j) = vertex;
		}
	}
}

void GeometryGenerator::Waves::Update(float dt)
{
	static float t = 0;

	t += dt;

	if (t >= mTimeStep)
	{
		for (UINT i = 1; i < mRows-1; i++)
		{
			for (UINT j = 1; j < mCols-1; j++)
			{
				mPrevVertices.at(i * mCols + j).mPosition.y =
					mK1 * mPrevVertices.at(i * mCols + j).mPosition.y +
					mK2 * mCurrVertices.at(i * mCols + j).mPosition.y +
					mK3 * (mCurrVertices.at((i + 1) * mCols + j).mPosition.y +
						   mCurrVertices.at((i - 1) * mCols + j).mPosition.y +
						   mCurrVertices.at(i * mCols + (j + 1)).mPosition.y +
						   mCurrVertices.at(i * mCols + (j - 1)).mPosition.y);
			}
		}

		// std::swap(mPrevVertices, mCurrVertices);

		for (UINT k = 0; k < mRows * mCols; k++)
		{
			Vertex v = mPrevVertices.at(k);
			mPrevVertices.at(k) = mCurrVertices.at(k);
			mCurrVertices.at(k) = v;
		}

		for (UINT i = 1; i < mRows - 1; i++)
		{
			for (UINT j = 1; j < mCols - 1; j++)
			{
				float l = mCurrVertices.at(i * mCols + (j - 1)).mPosition.y;
				float r = mCurrVertices.at(i * mCols + (j + 1)).mPosition.y;
				float t = mCurrVertices.at((i - 1) * mCols + j).mPosition.y;
				float b = mCurrVertices.at((i + 1) * mCols + j).mPosition.y;

				Vertex& v = mCurrVertices.at(i * mCols + j);

				v.mNormal.x = l - r;
				v.mNormal.y = 2 * mSpaceStep;
				v.mNormal.z = b - t;

				//v.mTexCoord.x = 0.5f + v.mPosition.x / (mCols * mSpaceStep);
				//v.mTexCoord.y = 0.5f - v.mPosition.z / (mRows * mSpaceStep);
			}
		}

		t = 0;
	}
}

void GeometryGenerator::Waves::Disturb(UINT i, UINT j, float magnitude)
{
	assert(i > 1 && i < mRows - 2);
	assert(j > 1 && j < mCols - 2);

	float HalfMagnitude = 0.5f * magnitude;

	mCurrVertices.at(i * mCols + j).mPosition.y += magnitude;
	mCurrVertices.at((i - 1) * mCols + j).mPosition.y += HalfMagnitude;
	mCurrVertices.at((i + 1) * mCols + j).mPosition.y += HalfMagnitude;
	mCurrVertices.at(i * mCols + (j - 1)).mPosition.y += HalfMagnitude;
	mCurrVertices.at(i * mCols + (j + 1)).mPosition.y += HalfMagnitude;
}

float GameMath::GetAngle2(XMFLOAT2 P)
{
	float theta = 0;
	
	if (P.x >= 0) // Quadrant I or IV
	{
		theta = std::atan(P.y / P.x); // in [-pi/2, +pi/2]

		if (theta < 0)
		{
			theta += XM_2PI; // in [0, 2*pi).
		}
	}
	else // Quadrant II or III
	{
		theta = std::atan(P.y / P.x) + XM_PI; // in [0, 2*pi).
	}

	return theta;
}

XMMATRIX GameMath::InverseTranspose(XMMATRIX M)
{
	M.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR det = XMMatrixDeterminant(M);
	return XMMatrixTranspose(XMMatrixInverse(&det, M));
}

void BlurEffect::Init(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	OnResize(device, width, height, format);
	
	// compile shaders
}

void BlurEffect::OnResize(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	SafeRelease(mOutputTextureSRV);
	//SafeRelease((*mOutputTextureSRV.GetAddressOf()));
	SafeRelease(mOutputTextureUAV);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	ID3D11Texture2D* texture = nullptr;
	
	// texture
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		HR(device->CreateTexture2D(&desc, nullptr, &texture)); 
	}

	// shader resource view
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = format;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = 1;

		//HR(device->CreateShaderResourceView(texture, &desc, mOutputTextureSRV.GetAddressOf()));
		HR(device->CreateShaderResourceView(texture, &desc, &mOutputTextureSRV));
	}

	// unordered access view
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Format = format;
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		HR(device->CreateUnorderedAccessView(texture, &desc, &mOutputTextureUAV));
	}

	SafeRelease(texture);
}

void BlurEffect::Blur(ID3D11DeviceContext* context, ID3D11ShaderResourceView* InputTextureSRV, ID3D11UnorderedAccessView* InputTextureUAV, UINT count)
{
	ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
	ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };

	for (UINT i = 0; i < count; ++i)
	{
		//context->VSSetShader(nullptr, nullptr, 0);
		//context->PSSetShader(nullptr, nullptr, 0);

		// horizontal blur
		//   input  : InputTextureSRV
		//   output : mOutputTextureUAV

		context->CSSetShader(mHorBlurCS, nullptr, 0);

		// bind
		context->CSSetShaderResources(0, 1, &InputTextureSRV);
		context->CSSetUnorderedAccessViews(0, 1, &mOutputTextureUAV, nullptr);

		UINT GroupsX = std::ceil(mWidth / 256.0f);
		context->Dispatch(GroupsX, mHeight, 1);

		// unbind
		context->CSSetShaderResources(0, 1, NullSRV);
		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);

		// vertical blur
		//   input  : mOutputTextureSRV
		//   output : InputTextureUAV

		context->CSSetShader(mVerBlurCS, nullptr, 0);

		// bind
		//context->CSSetShaderResources(0, 1, mOutputTextureSRV.GetAddressOf());
		context->CSSetShaderResources(0, 1, &mOutputTextureSRV);
		context->CSSetUnorderedAccessViews(0, 1, &InputTextureUAV, nullptr);

		UINT GroupsY = std::ceil(mHeight / 256.0f);
		context->Dispatch(mWidth, GroupsY, 1);

		// unbind
		context->CSSetShaderResources(0, 1, NullSRV);
		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
	}

	context->CSSetShader(nullptr, nullptr, 0);
}

//void BlurEffect::SetGaussianWeights(float sigma)
//{
//	float d = 2.0f * sigma * sigma;
//
//	float weights[9];
//	float sum = 0.0f;
//	for (int i = 0; i < 8; ++i)
//	{
//		float x = (float)i;
//		weights[i] = expf(-x * x / d);
//
//		sum += weights[i];
//	}
//
//	// Divide by the sum so all the weights add up to 1.0.
//	for (int i = 0; i < 8; ++i)
//	{
//		weights[i] /= sum;
//	}
//
//	Effects::BlurFX->SetWeights(weights);
//}

CameraObject::CameraObject() :
	mPosition(0, 0, 0),
	mRight(1, 0, 0),
	mUp(0, 1, 0),
	mLook(0, 0, 1)
{}

void CameraObject::SetFrustum(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
{
	mFovAngleY = FovAngleY;
	mAspectRatio = AspectRatio;
	mNearZ = NearZ;
	mFarZ = FarZ;

	mProj = XMMatrixPerspectiveFovLH(mFovAngleY, mAspectRatio, mNearZ, mFarZ);
	BoundingFrustum::CreateFromMatrix(mFrustum, mProj);
}


void CameraObject::LookAt(const XMFLOAT3& position, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&position);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	XMVECTOR L = XMVector3Normalize(T - P);
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(U, L));

	XMStoreFloat3(&mPosition, P);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);

	{
		XMVECTOR U = XMVector3Cross(L, R);
		XMStoreFloat3(&mUp, U);
	}
}

void CameraObject::UpdateView()
{
	XMVECTOR R = XMLoadFloat3(&mRight);
	XMVECTOR U = XMLoadFloat3(&mUp);
	XMVECTOR L = XMLoadFloat3(&mLook);
	XMVECTOR P = XMLoadFloat3(&mPosition);

	L = XMVector3Normalize(L);
	U = XMVector3Normalize(XMVector3Cross(L, R));
	R = XMVector3Cross(U, L);
	
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);
	XMStoreFloat3(&mLook, L);

	float x = -XMVectorGetX(XMVector3Dot(P, R));
	float y = -XMVectorGetY(XMVector3Dot(P, U));
	float z = -XMVectorGetZ(XMVector3Dot(P, L));

	mView(0, 0) = mRight.x;
	mView(1, 0) = mRight.y;
	mView(2, 0) = mRight.z;
	mView(3, 0) = x;

	mView(0, 1) = mUp.x;
	mView(1, 1) = mUp.y;
	mView(2, 1) = mUp.z;
	mView(3, 1) = y;

	mView(0, 2) = mLook.x;
	mView(1, 2) = mLook.y;
	mView(2, 2) = mLook.z;
	mView(3, 2) = z;

	mView(0, 3) = 0.0f;
	mView(1, 3) = 0.0f;
	mView(2, 3) = 0.0f;
	mView(3, 3) = 1.0f;
}

void CameraObject::walk(float delta)
{
	// mPosition += delta*mLook
	XMVECTOR d = XMVectorReplicate(delta);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(d, l, p));
}

void CameraObject::strafe(float delta)
{
	// mPosition += delta*mRight
	XMVECTOR d = XMVectorReplicate(delta);
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(d, r, p));
}

void CameraObject::pitch(float angle)
{
	// rotate up and look vector about the right vector

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
}

void CameraObject::rotate(float angle)
{
	// rotate the basis vectors about the world y-axis

	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
}


DynamicCubeMap::DynamicCubeMap() :
	mDSV(nullptr),
	mSRV(nullptr)
{
	for (UINT i = 0; i < 6; ++i)
	{
		mRTV[i] = nullptr;
	}

	mCubeMapSize = 256;

	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = mCubeMapSize;
	mViewport.Height = mCubeMapSize;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;
}

DynamicCubeMap::~DynamicCubeMap()
{
	SafeRelease(mDSV);

	for (UINT i = 0; i < 6; ++i)
	{
		SafeRelease(mRTV[i]);
	}

	SafeRelease(mSRV);
}

void DynamicCubeMap::Init(ID3D11Device* device, const XMFLOAT3& P)
{
	// RTV & SRV
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mCubeMapSize;
		desc.Height = mCubeMapSize;
		desc.MipLevels = 0;
		desc.ArraySize = 6;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		// RTV
		{
			D3D11_RENDER_TARGET_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = 1;
			desc.Texture2DArray.MipSlice = 0;

			for (UINT i = 0; i < 6; ++i)
			{
				desc.Texture2DArray.FirstArraySlice = i;
				HR(device->CreateRenderTargetView(texture, &desc, &mRTV[i]));
			}
		}

		// SRV
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			desc.TextureCube.MostDetailedMip = 0;
			desc.TextureCube.MipLevels = -1;

			HR(device->CreateShaderResourceView(texture, &desc, &mSRV));
		}

		SafeRelease(texture);
	}

	// DSV
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mCubeMapSize;
		desc.Height = mCubeMapSize;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		// DSV
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_D32_FLOAT;
			desc.Flags = 0;
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = 0;
			HR(device->CreateDepthStencilView(texture, &desc, &mDSV));
		}

		SafeRelease(texture);
	}

	// camera
	{
		XMFLOAT3 T[6] =
		{
			XMFLOAT3(P.x + 1, P.y, P.z), // +X
			XMFLOAT3(P.x - 1, P.y, P.z), // -X
			XMFLOAT3(P.x, P.y + 1, P.z), // +Y
			XMFLOAT3(P.x, P.y - 1, P.z), // -Y
			XMFLOAT3(P.x, P.y, P.z + 1), // +Z
			XMFLOAT3(P.x, P.y, P.z - 1), // -Z
		};

		XMFLOAT3 U[6] =
		{
			XMFLOAT3(0, +1, 0), // +X
			XMFLOAT3(0, +1, 0), // -X
			XMFLOAT3(0, 0, -1), // +Y
			XMFLOAT3(0, 0, +1), // -Y
			XMFLOAT3(0, +1, 0), // +Z
			XMFLOAT3(0, +1, 0), // -Z
		};

		for (UINT i = 0; i < 6; ++i)
		{
			mCamera[i].LookAt(P, T[i], U[i]);
			mCamera[i].SetFrustum(XM_PIDIV2, 1, 0.1f, 1000);
			mCamera[i].UpdateView();
		}
	}
}

//void DynamicCubeMap::Draw() {}


DebugQuad::DebugQuad() :
	mDebugQuadCB(nullptr)
{}

DebugQuad::~DebugQuad()
{
	SafeRelease(mDebugQuadCB);
}

void DebugQuad::Init(ID3D11Device* device, float WindowAspectRatio, ScreenCorner position, float TextureAspectRatio)
{
	GeometryGenerator::CreateScreenQuad(mMesh);

	// vertex buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mMesh.mVertices.size();
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = mMesh.mVertices.data();
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		HR(device->CreateBuffer(&desc, &InitData, &mVertexBuffer));
	}

	// input buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(UINT) * mMesh.mIndices.size();
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = mMesh.mIndices.data();
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		HR(device->CreateBuffer(&desc, &InitData, &mIndexBuffer));
	}

	// vertex shader
	{
		std::wstring path = L"DebugQuadVS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mVertexShader));

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};
			HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mInputLayout));
		}
	}

	// pixel shader
	{
		std::wstring path = L"DebugQuadPS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mPixelShader));
	}

	// constant buffer
	{
		static_assert((sizeof(DebugQuadCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(DebugQuadCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(device->CreateBuffer(&desc, nullptr, &mDebugQuadCB));
	}

	mPosition = position;
	mAspectRatio = TextureAspectRatio;

	OnResize(WindowAspectRatio);
}

void DebugQuad::OnResize(float WindowAspectRatio)
{
	float w = (0.5f / WindowAspectRatio) * mAspectRatio;

	XMMATRIX S = XMMatrixScaling(w, 0.5f, 1.0f);
	XMMATRIX T;

	switch (mPosition)
	{
		case TopLeft:
			T = XMMatrixTranslation(-1 + w, +0.5f, 0.0f);
			break;
		case TopRight:
			T = XMMatrixTranslation(+1 - w, +0.5f, 0.0f);
			break;
		case BottomLeft:
			T = XMMatrixTranslation(-1 + w, -0.5f, 0.0f);
			break;
		case BottomRight:
			T = XMMatrixTranslation(+1 - w, -0.5f, 0.0f);
			break;
	}

	mWorld = S * T;
}

void DebugQuad::Draw(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv)
{
	// shaders
	context->VSSetShader(mVertexShader.Get(), nullptr, 0);
	context->PSSetShader(mPixelShader.Get(), nullptr, 0);

	// input layout
	context->IASetInputLayout(mInputLayout.Get());

	// primitive topology
	context->IASetPrimitiveTopology(mPrimitiveTopology);

	// vertex and index buffers
	UINT stride = sizeof(GeometryGenerator::Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// constant buffer per object
	{
		DebugQuadCB buffer;
		XMStoreFloat4x4(&buffer.mWorldViewProj, mWorld);
		context->UpdateSubresource(mDebugQuadCB, 0, nullptr, &buffer, 0, 0);
		context->VSSetConstantBuffers(0, 1, &mDebugQuadCB);
	}

	// rasterizer, blend and depth-stencil states
	context->RSSetState(mRasterizerState.Get());
	FLOAT BlendFactor[] = { 0, 0, 0, 0 };
	context->OMSetBlendState(mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
	context->OMSetDepthStencilState(mDepthStencilState.Get(), mStencilRef);

	// bind SRV
	context->PSSetShaderResources(0, 1, &srv);

	// draw call
	context->DrawIndexed(mMesh.mIndices.size(), mIndexStart, mVertexStart);

	// unbind SRV
	ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
	context->PSSetShaderResources(0, 1, NullSRV);
}

ShadowMap::ShadowMap() :
	mWidth(0),
	mHeight(0),
	mDSV(nullptr),
	mSRV(nullptr),
	mPerObjectCB(nullptr),
	mVertexShader{ nullptr, nullptr },
	mInputLayout{ nullptr, nullptr },
	mPixelShader{ nullptr, nullptr },
	mRasterizerState(nullptr),
	mSamplerState(nullptr)
{}

ShadowMap::~ShadowMap()
{
	SafeRelease(mDSV);
	SafeRelease(mSRV);
	SafeRelease(mPerObjectCB);

	for (UINT i = 0; i < 2; ++i)
	{
		SafeRelease(mVertexShader[i]);
		SafeRelease(mInputLayout[i]);
		SafeRelease(mPixelShader[i]);
	}
	
	SafeRelease(mRasterizerState);
	SafeRelease(mSamplerState);
}
void ShadowMap::Init(ID3D11Device* device, UINT width, UINT height)
{
	mWidth = width;
	mHeight = height;

	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = width;
	mViewport.Height = height;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = mWidth;
	desc.Height = mHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	ID3D11Texture2D* texture = nullptr;
	HR(device->CreateTexture2D(&desc, nullptr, &texture));

	// DSV
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Flags = 0;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		HR(device->CreateDepthStencilView(texture, &desc, &mDSV));
	}

	// SRV
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.MostDetailedMip = 0;

		HR(device->CreateShaderResourceView(texture, &desc, &mSRV));
	}

	SafeRelease(texture);

	// per object constant buffer
	{

		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerObjectCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(device->CreateBuffer(&desc, nullptr, &mPerObjectCB));
	}

	// vertex shaders
	for (UINT i = 0; i < 2; ++i)
	{
		std::wstring path = i == 0 ? L"ShadowMapVS.hlsl" : L"ShadowMapSkinnedVS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mVertexShader[i]));

		// input layout
		if (i == 0)
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mInputLayout[i]));
		}
		else
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"WEIGHTS",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0}
			};

			HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mInputLayout[i]));
		}
	}

	// PS
	for (UINT i = 0; i < 2; ++i)
	{
		std::wstring path = L"ShadowMapPS.hlsl";

		std::string str = std::to_string(i);

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_TEXTURE",         "1" });
		defines.push_back({ "ENABLE_ALPHA_CLIPPING",  str.c_str() });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mPixelShader[i]));
	}

	// rasterizer state
	{
		D3D11_RASTERIZER_DESC desc;
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = false;
		desc.DepthBias = 100000;
		desc.DepthBiasClamp = 0;
		desc.SlopeScaledDepthBias = 1;
		desc.DepthClipEnable = true;
		desc.ScissorEnable = false;
		desc.MultisampleEnable = false;
		desc.AntialiasedLineEnable = false;

		HR(device->CreateRasterizerState(&desc, &mRasterizerState));
	}

	// sampler state
	{
		D3D11_SAMPLER_DESC desc;
		desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.MipLODBias = 0;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_LESS;
		ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
		desc.MinLOD = 0;
		desc.MaxLOD = 0;

		HR(device->CreateSamplerState(&desc, &mSamplerState));
	}
}

ID3D11ShaderResourceView*& ShadowMap::GetSRV()
{
	return mSRV;
}

void ShadowMap::BindDSVAndSetNullRenderTarget(ID3D11DeviceContext* context)
{
	context->RSSetViewports(1, &mViewport);
	context->OMSetRenderTargets(0, nullptr, mDSV);
	context->ClearDepthStencilView(mDSV, D3D11_CLEAR_DEPTH, 1, 0);
}

void ShadowMap::BuildTranform(const XMFLOAT3& light, const BoundingSphere& bounds)
{
	XMVECTOR dir = XMLoadFloat3(&light);
	XMVECTOR pos = -2 * bounds.Radius * dir;
	XMVECTOR target = XMLoadFloat3(&bounds.Center);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	// light space transform
	XMMATRIX V = XMMatrixLookAtLH(pos, target, up);

	// transform bounding sphere to light space
	XMFLOAT3 SphereCenterLS;
	XMStoreFloat3(&SphereCenterLS, XMVector3TransformCoord(target, V));

	// ortho frustum in light space encloses bounds
	float l = SphereCenterLS.x - bounds.Radius;
	float b = SphereCenterLS.y - bounds.Radius;
	float n = SphereCenterLS.z - bounds.Radius;
	float r = SphereCenterLS.x + bounds.Radius;
	float t = SphereCenterLS.y + bounds.Radius;
	float f = SphereCenterLS.z + bounds.Radius;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T
	(
		+0.5f,  0.0f, 0.0f, 0.0f,
		 0.0f, -0.5f, 0.0f, 0.0f,
		 0.0f,  0.0f, 1.0f, 0.0f,
		+0.5f, +0.5f, 0.0f, 1.0f
	);

	XMMATRIX S = V * P * T;

	XMStoreFloat4x4(&mLightView, V);
	XMStoreFloat4x4(&mLightProj, P);
	XMStoreFloat4x4(&mShadowTransform, S);
}

ID3D11VertexShader* ShadowMap::GetVS(bool IsSkinned)
{
	return mVertexShader[IsSkinned ? 1 : 0];
}

ID3D11InputLayout* ShadowMap::GetIL(bool IsSkinned)
{
	return mInputLayout[IsSkinned ? 1 : 0];
}

ID3D11PixelShader* ShadowMap::GetPS(bool IsAlphaClipping)
{
	return mPixelShader[IsAlphaClipping ? 1 : 0];
}

ID3D11RasterizerState* ShadowMap::GetRS()
{
	return mRasterizerState;
}

ID3D11SamplerState*& ShadowMap::GetSS()
{
	return mSamplerState;
}

ID3D11Buffer*& ShadowMap::GetCB()
{
	return mPerObjectCB;
}


SSAO::SSAO() :
	mNormalDepthRTV(nullptr),
	mNormalDepthSRV(nullptr),
	mNormalDepthVS{ nullptr, nullptr },
	mNormalDepthIL{ nullptr, nullptr },
	mNormalDepthPS{ nullptr, nullptr },
	mNormalDepthSS(nullptr),
	mRandomVectorSRV(nullptr),
	mRandomVectorSS(nullptr),
	mAmbientMapRTV{nullptr, nullptr},
	mAmbientMapSRV{nullptr, nullptr},
	mAmbientMapComputeCB(nullptr),
	mBlurVS(nullptr),
	mBlurIL(nullptr),
	mBlurPS{ nullptr, nullptr },
	mBlurCB(nullptr),
	mBlurSS(nullptr)
{}

SSAO::~SSAO()
{
	SafeRelease(mNormalDepthRTV);
	SafeRelease(mNormalDepthSRV);
	for (UINT i = 0; i < 2; ++i)
	{
		SafeRelease(mNormalDepthVS[i]);
		SafeRelease(mNormalDepthIL[i]);
		SafeRelease(mNormalDepthPS[i]);
	}
	SafeRelease(mNormalDepthCB);
	SafeRelease(mNormalDepthSS);
	SafeRelease(mRandomVectorSRV);
	SafeRelease(mRandomVectorSS);
	SafeRelease(mAmbientMapRTV[0]);
	SafeRelease(mAmbientMapSRV[0]);
	SafeRelease(mAmbientMapRTV[1]);
	SafeRelease(mAmbientMapSRV[1]);
	SafeRelease(mAmbientMapComputeCB);
	SafeRelease(mBlurVS);
	SafeRelease(mBlurIL);
	SafeRelease(mBlurPS[0]);
	SafeRelease(mBlurPS[1]);
	SafeRelease(mBlurCB);
	SafeRelease(mBlurSS);
}

void SSAO::Init(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float FarZ)
{
	OnResize(device, width, height, FieldOfViewY, FarZ);

	// normal depth
	{
		// VS
		for (UINT i = 0; i < 2; ++i)
		{
			std::wstring path = i == 0 ? L"SSAONormalDepthVS.hlsl" : L"SSAONormalDepthSkinnedVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mNormalDepthVS[i]));

			// input layout
			if (i == 0)
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mNormalDepthIL[i]));
			}
			else
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"WEIGHTS",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0}
				};

				HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mNormalDepthIL[i]));
			}
		}

		// PS
		for (UINT i = 0; i < 2; ++i)
		{
			std::wstring path = L"SSAONormalDepthPS.hlsl";

			std::string str = std::to_string(i);

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "1" });
			defines.push_back({ "ENABLE_ALPHA_CLIPPING",  str.c_str() });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mNormalDepthPS[i]));
		}

		// constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(NormalDepthCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(device->CreateBuffer(&desc, nullptr, &mNormalDepthCB));
		}

		// sampler state
		{
			D3D11_SAMPLER_DESC desc;
			desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MipLODBias = 0;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			FLOAT BorderColor[4] = { 0.0f, 0.0f, 0.0f, 1e5f };
			CopyMemory(desc.BorderColor, BorderColor, sizeof(desc.BorderColor));
			desc.MinLOD = 0;
			desc.MaxLOD = 0;

			HR(device->CreateSamplerState(&desc, &mNormalDepthSS));
		}
	}

	// samples offset
	{
		// 14 uniformly distributed vectors :
		//  the 8 corners of the cube and
		//  the 6 center points along each cube face
		
		// alternate the points on opposites sides of the cubes to
		// get the vectors spread out even with less than 14 samples

		// 8 cube corners
		mSampleOffset[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
		mSampleOffset[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

		mSampleOffset[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
		mSampleOffset[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

		mSampleOffset[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
		mSampleOffset[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

		mSampleOffset[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
		mSampleOffset[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

		// 6 centers of cube faces
		mSampleOffset[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
		mSampleOffset[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

		mSampleOffset[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
		mSampleOffset[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

		mSampleOffset[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
		mSampleOffset[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

		for (UINT i = 0; i < 14; ++i)
		{
			// random length in [0.25, 1.0)
			float length = GameMath::RandNorm(0.25f, 1.0f);

			XMVECTOR v = length * XMVector4Normalize(XMLoadFloat4(&mSampleOffset[i]));
			XMStoreFloat4(&mSampleOffset[i], v);
		}
	}

	// random vector map
	{
		// SRV
		{
			D3D11_TEXTURE2D_DESC desc;
			desc.Width = 256;
			desc.Height = 256;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			DirectX::PackedVector::XMCOLOR color[256 * 256];
			for (UINT i = 0; i < 256; ++i)
			{
				for (UINT j = 0; j < 256; ++j)
				{
					XMFLOAT3 v(GameMath::RandNorm(), GameMath::RandNorm(), GameMath::RandNorm());
					color[i * 256 + j] = DirectX::PackedVector::XMCOLOR(v.x, v.y, v.z, 0.0f);
				}
			}

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = color;
			InitData.SysMemPitch = 256 * sizeof(DirectX::PackedVector::XMCOLOR);
			InitData.SysMemSlicePitch = 0;

			ID3D11Texture2D* texture = nullptr;
			HR(device->CreateTexture2D(&desc, &InitData, &texture));

			HR(device->CreateShaderResourceView(texture, nullptr, &mRandomVectorSRV));

			SafeRelease(texture);
		}

		// sampler state
		{
			D3D11_SAMPLER_DESC desc;
			desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MipLODBias = 0;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
			desc.MinLOD = 0;
			desc.MaxLOD = 0;

			HR(device->CreateSamplerState(&desc, &mRandomVectorSS));
		}
	}

	// ambient map
	{
		GeometryGenerator::CreateScreenQuad(mAmbientMapQuad.mMesh);

		// store far plane frustum corner indices in normal.x
		mAmbientMapQuad.mMesh.mVertices[0].mNormal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mAmbientMapQuad.mMesh.mVertices[1].mNormal = XMFLOAT3(1.0f, 0.0f, 0.0f);
		mAmbientMapQuad.mMesh.mVertices[2].mNormal = XMFLOAT3(2.0f, 0.0f, 0.0f);
		mAmbientMapQuad.mMesh.mVertices[3].mNormal = XMFLOAT3(3.0f, 0.0f, 0.0f);

		// vertex buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mAmbientMapQuad.mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = mAmbientMapQuad.mMesh.mVertices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(device->CreateBuffer(&desc, &InitData, &mAmbientMapQuad.mVertexBuffer));
		}

		// input buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * mAmbientMapQuad.mMesh.mIndices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = mAmbientMapQuad.mMesh.mIndices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(device->CreateBuffer(&desc, &InitData, &mAmbientMapQuad.mIndexBuffer));
		}

		// VS
		{
			std::wstring path = L"SSAOAmbientMapComputeVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mAmbientMapQuad.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mAmbientMapQuad.mInputLayout));
			}
		}

		// PS
		{
			std::wstring path = L"SSAOAmbientMapComputePS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mAmbientMapQuad.mPixelShader));
		}

		// constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(AmbientMapComputeCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(device->CreateBuffer(&desc, nullptr, &mAmbientMapComputeCB));
		}
	}

	// blur
	{
		// VS
		{
			std::wstring path = L"SSAOBlurVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mBlurVS));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mBlurIL));
			}
		}

		// PS
		{
			std::wstring path = L"SSAOBlurPS.hlsl";

			for (UINT i = 0; i < 2; ++i)
			{
				std::string str = std::to_string(i);

				std::vector<D3D_SHADER_MACRO> defines;
				defines.push_back({ "HORIZONTAL_BLUR", str.c_str() });
				defines.push_back({ nullptr, nullptr });
				
				ID3DBlob* pCode;
				HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
				HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mBlurPS[i]));
			}
		}

		// constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(BlurCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(device->CreateBuffer(&desc, nullptr, &mBlurCB));
		}

		// sampler state
		{
			D3D11_SAMPLER_DESC desc;
			desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MipLODBias = 0;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
			desc.MinLOD = 0;
			desc.MaxLOD = 0;

			HR(device->CreateSamplerState(&desc, &mBlurSS));
		}
	}
}

void SSAO::OnResize(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float FarZ)
{
	mWidth = width;
	mHeight = height;

	// render to ambient map at half the resolution
	mAmbientMapViewport.TopLeftX = 0.0f;
	mAmbientMapViewport.TopLeftY = 0.0f;
	mAmbientMapViewport.Width = mWidth / 2.0f;
	mAmbientMapViewport.Height = mHeight / 2.0f;
	mAmbientMapViewport.MinDepth = 0.0f;
	mAmbientMapViewport.MaxDepth = 1.0f;

	// frustum far corners
	{
		float aspect = (float)mWidth / (float)mHeight;

		float halfHeight = FarZ * tanf(0.5f * FieldOfViewY);
		float halfWidth = aspect * halfHeight;

		mFrustumFarCorner[0] = XMFLOAT4(-halfWidth, -halfHeight, FarZ, 0.0f);
		mFrustumFarCorner[1] = XMFLOAT4(-halfWidth, +halfHeight, FarZ, 0.0f);
		mFrustumFarCorner[2] = XMFLOAT4(+halfWidth, +halfHeight, FarZ, 0.0f);
		mFrustumFarCorner[3] = XMFLOAT4(+halfWidth, -halfHeight, FarZ, 0.0f);
	}

	// normal depth RTV and SRV
	{
		SafeRelease(mNormalDepthRTV);
		SafeRelease(mNormalDepthSRV);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWidth;
		desc.Height = mHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		HR(device->CreateRenderTargetView(texture, nullptr, &mNormalDepthRTV));
		HR(device->CreateShaderResourceView(texture, nullptr, &mNormalDepthSRV));

		SafeRelease(texture);
	}

	// ambient map RTV and SRV
	{
		// render to ambient map at half the resolution
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWidth / 2;
		desc.Height = mHeight / 2;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R16_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		for (UINT i = 0; i < 2; ++i)
		{
			SafeRelease(mAmbientMapRTV[i]);
			SafeRelease(mAmbientMapSRV[i]);

			ID3D11Texture2D* texture = nullptr;
			HR(device->CreateTexture2D(&desc, nullptr, &texture));

			HR(device->CreateRenderTargetView(texture, nullptr, &mAmbientMapRTV[i]));
			HR(device->CreateShaderResourceView(texture, nullptr, &mAmbientMapSRV[i]));

			SafeRelease(texture);
		}
	}
}

ID3D11Buffer*& SSAO::GetNormalDepthCB()
{
	return mNormalDepthCB;
}

ID3D11VertexShader* SSAO::GetNormalDepthVS(bool IsSkinned)
{
	return mNormalDepthVS[IsSkinned ? 1 : 0];
}

ID3D11InputLayout* SSAO::GetNormalDepthIL(bool IsSkinned)
{
	return mNormalDepthIL[IsSkinned ? 1 : 0];
}

ID3D11PixelShader* SSAO::GetNormalDepthPS(bool IsAlphaClipping)
{
	return mNormalDepthPS[IsAlphaClipping ? 1 : 0];
}

void SSAO::BindNormalDepthRenderTarget(ID3D11DeviceContext* context, ID3D11DepthStencilView* dsv)
{
	context->OMSetRenderTargets(1, &mNormalDepthRTV, dsv);

	// clear view space normal to (0,0,-1) and clear view space depth to be very far away
	float ClearColor[] = { 0.0f, 0.0f, -1.0f, 1e5f };
	context->ClearRenderTargetView(mNormalDepthRTV, ClearColor);
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
}

void SSAO::ComputeAmbientMap(ID3D11DeviceContext* context, const CameraObject& camera)
{
	// bind the ambient map as the render target
	// do not bind a depth/stencil buffer -> no depth test is performed
	context->OMSetRenderTargets(1, &mAmbientMapRTV[0], nullptr);
	context->ClearRenderTargetView(mAmbientMapRTV[0], Colors::Black);
	context->RSSetViewports(1, &mAmbientMapViewport);

	// transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T
	(
		+0.5f,  0.0f, 0.0f, 0.0f,
		 0.0f, -0.5f, 0.0f, 0.0f,
		 0.0f,  0.0f, 1.0f, 0.0f,
		+0.5f, +0.5f, 0.0f, 1.0f
	);

	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		context->VSSetShader(mAmbientMapQuad.mVertexShader.Get(), nullptr, 0);
		context->PSSetShader(mAmbientMapQuad.mPixelShader.Get(), nullptr, 0);

		// input layout
		context->IASetInputLayout(mAmbientMapQuad.mInputLayout.Get());

		// primitive topology
		context->IASetPrimitiveTopology(mAmbientMapQuad.mPrimitiveTopology);

		// vertex and index buffers
		if (mAmbientMapQuad.mInstancedBuffer)
		{
			UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
			UINT offset[2] = { 0, 0 };

			ID3D11Buffer* vbs[2] = { mAmbientMapQuad.mVertexBuffer.Get(), mAmbientMapQuad.mInstancedBuffer };

			context->IASetVertexBuffers(0, 2, vbs, stride, offset);
			context->IASetIndexBuffer(mAmbientMapQuad.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}
		else
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			context->IASetVertexBuffers(0, 1, mAmbientMapQuad.mVertexBuffer.GetAddressOf(), &stride, &offset);
			context->IASetIndexBuffer(mAmbientMapQuad.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		// constant buffer per object
		{
			AmbientMapComputeCB buffer;
			XMStoreFloat4x4(&buffer.ProjTexture, camera.mProj * T);
			for (UINT i = 0; i <  4; ++i) buffer.FrustumFarCorner[i] = mFrustumFarCorner[i];
			for (UINT i = 0; i < 14; ++i) buffer.SampleOffset[i] = mSampleOffset[i];
			buffer.OcclusionRadius = 0.5f;
			buffer.OcclusionFadeStart = 0.2f;
			buffer.OcclusionFadeEnd = 2.0f;
			buffer.SurfaceEpsilon = 0.05f;

			context->UpdateSubresource(mAmbientMapComputeCB, 0, 0, &buffer, 0, 0);

			context->VSSetConstantBuffers(0, 1, &mAmbientMapComputeCB);
			context->PSSetConstantBuffers(0, 1, &mAmbientMapComputeCB);
		}

		// bind SRVs
		{
			context->PSSetShaderResources(0, 1, &mNormalDepthSRV);
			context->PSSetShaderResources(1, 1, &mRandomVectorSRV);
		}

		// bind sampler states
		{
			context->PSSetSamplers(2, 1, &mNormalDepthSS);
			context->PSSetSamplers(3, 1, &mRandomVectorSS);
		}

		// rasterizer, blend and depth-stencil states
		context->RSSetState(mAmbientMapQuad.mRasterizerState.Get());
		context->OMSetBlendState(mAmbientMapQuad.mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		context->OMSetDepthStencilState(mAmbientMapQuad.mDepthStencilState.Get(), mAmbientMapQuad.mStencilRef);

		// draw call
		if (mAmbientMapQuad.mIndexBuffer && mAmbientMapQuad.mInstancedBuffer)
		{
			context->DrawIndexedInstanced(mAmbientMapQuad.mMesh.mIndices.size(), mAmbientMapQuad.mVisibleInstanceCount, 0, 0, 0);
		}
		else if (mAmbientMapQuad.mIndexBuffer)
		{
			context->DrawIndexed(mAmbientMapQuad.mMesh.mIndices.size(), mAmbientMapQuad.mIndexStart, mAmbientMapQuad.mVertexStart);
		}
		else
		{
			context->Draw(mAmbientMapQuad.mMesh.mVertices.size(), mAmbientMapQuad.mVertexStart);
		}

		//// unbind SRVs
		ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
		context->PSSetShaderResources(0, 2, NullSRV);
	}

	context->OMSetRenderTargets(0, nullptr, nullptr);
}

void SSAO::BlurAmbientMap(ID3D11DeviceContext* context, UINT count)
{
	// viewport
	context->RSSetViewports(1, &mAmbientMapViewport);

	// vertex shader
	context->VSSetShader(mBlurVS, nullptr, 0);

	// input layout
	context->IASetInputLayout(mBlurIL);

	// primitive topology
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// vertex and index buffers
	{
		UINT stride = sizeof(GeometryGenerator::Vertex);
		UINT offset = 0;

		context->IASetVertexBuffers(0, 1, mAmbientMapQuad.mVertexBuffer.GetAddressOf(), &stride, &offset);
		context->IASetIndexBuffer(mAmbientMapQuad.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	// constant buffer
	{
		BlurCB buffer;
		buffer.TexelWidth = 1.0f / mAmbientMapViewport.Width;
		buffer.TexelHeight = 1.0f / mAmbientMapViewport.Height;

		context->UpdateSubresource(mBlurCB, 0, 0, &buffer, 0, 0);
		context->PSSetConstantBuffers(0, 1, &mBlurCB);
	}

	// bind sampler state
	context->PSSetSamplers(4, 1, &mBlurSS);

	for (UINT i = 0; i < count; ++i)
	{
		// ping-pong ambient map textures for horizontal and vertical blur
		BlurAmbientMap(context, mAmbientMapRTV[1], mAmbientMapSRV[0], true);
		BlurAmbientMap(context, mAmbientMapRTV[0], mAmbientMapSRV[1], false);
	}

	// unbind sampler state
	ID3D11SamplerState* NullSS[] = { nullptr };
	context->PSSetSamplers(4, 1, NullSS);
}

void SSAO::BlurAmbientMap(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv, ID3D11ShaderResourceView* srv, bool HorizontalBlur)
{
	// render target
	context->OMSetRenderTargets(1, &rtv, nullptr);
	context->ClearRenderTargetView(rtv, Colors::Black);

	// pixel shader
	context->PSSetShader(mBlurPS[HorizontalBlur ? 1 : 0], nullptr, 0);
	
	// bind SRVs
	{
		ID3D11ShaderResourceView* SRVs[2] = { mNormalDepthSRV, srv };
		context->PSSetShaderResources(0, 2, SRVs);
	}

	context->DrawIndexed(mAmbientMapQuad.mMesh.mIndices.size(), mAmbientMapQuad.mIndexStart, mAmbientMapQuad.mVertexStart);

	// unbind SRVs
	{
		ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
		context->PSSetShaderResources(0, 2, NullSRVs);
	}

	context->OMSetRenderTargets(0, nullptr, nullptr);
}

ID3D11ShaderResourceView*& SSAO::GetNormalDepthSRV()
{
	return mNormalDepthSRV;
}

ID3D11ShaderResourceView*& SSAO::GetAmbientMapSRV()
{
	return mAmbientMapSRV[0];
}

ID3D11RenderTargetView*& SSAO::GetAmbientMapRTV()
{
	return mAmbientMapRTV[0];
}


AnimationObject::KeyFrame::KeyFrame() :
	time(0),
	translation(0, 0, 0),
	scale(1, 1, 1),
	rotation(0, 0, 0, 1)
{}

AnimationObject::KeyFrame::~KeyFrame()
{}


float AnimationObject::GetTimeStart() const
{
	return keyframes.front().time;
}

float AnimationObject::GetTimeEnd() const
{
	return keyframes.back().time;
}

void AnimationObject::interpolate(float t, XMMATRIX& world) const
{
	// rotation origin
	XMVECTOR O = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	if (t <= GetTimeStart()) // return first keyframe
	{
		XMVECTOR S = XMLoadFloat3(&keyframes.front().scale);
		XMVECTOR R = XMLoadFloat4(&keyframes.front().rotation);
		XMVECTOR T = XMLoadFloat3(&keyframes.front().translation);

		world = XMMatrixAffineTransformation(S, O, R, T);
	}
	else if (t >= GetTimeEnd()) // return last keyframe
	{
		XMVECTOR S = XMLoadFloat3(&keyframes.back().scale);
		XMVECTOR R = XMLoadFloat4(&keyframes.back().rotation);
		XMVECTOR T = XMLoadFloat3(&keyframes.back().translation);

		world = XMMatrixAffineTransformation(S, O, R, T);
	}
	else // interpolate keyframes
	{
		for (UINT i = 0; i < keyframes.size() - 1; ++i)
		{
			const KeyFrame& a = keyframes[i + 0];
			const KeyFrame& b = keyframes[i + 1];

			if (a.time <= t && t <= b.time)
			{
				XMVECTOR Sa = XMLoadFloat3(&a.scale);
				XMVECTOR Sb = XMLoadFloat3(&b.scale);

				XMVECTOR Ra = XMLoadFloat4(&a.rotation);
				XMVECTOR Rb = XMLoadFloat4(&b.rotation);

				XMVECTOR Ta = XMLoadFloat3(&a.translation);
				XMVECTOR Tb = XMLoadFloat3(&b.translation);

				float x = (t - a.time) / (b.time - a.time);

				XMVECTOR S = XMVectorLerp(Sa, Sb, x);
				XMVECTOR R = XMQuaternionSlerp(Ra, Rb, x);
				XMVECTOR T = XMVectorLerp(Ta, Tb, x);

				world = XMMatrixAffineTransformation(S, O, R, T);
				break;
			}
		}
	}
}

float AnimationClip::GetTimeClipStart() const
{
	float t = FLT_MAX;

	for (const AnimationObject& animation : mAnimationObjects)
	{
		t = std::min(t, animation.GetTimeStart());
	}

	return t;
}

float AnimationClip::GetTimeClipEnd() const
{
	float t = 0;

	for (const AnimationObject& animation : mAnimationObjects)
	{
		t = std::max(t, animation.GetTimeEnd());
	}

	return t;
}

void AnimationClip::interpolate(float t, std::vector<XMMATRIX>& transforms) const
{
	transforms.clear();
	transforms.resize(mAnimationObjects.size());

	for (UINT i = 0; i < mAnimationObjects.size(); ++i)
	{
		mAnimationObjects[i].interpolate(t, transforms[i]);
	}
}

TextureManager::TextureManager() :
	mDevice(nullptr),
	mContext(nullptr)
{
	//mTextureFolder = L"C:/Users/ggarbin/Desktop/3D-Game-Programming-with-DirectX11/textures/";
	mTextureFolder = L"C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/textures/";
}

TextureManager::~TextureManager()
{
	for (auto& [filename, srv] : mSRVs)
	{
		SafeRelease(srv);
	}

	mDevice = nullptr;
	mSRVs.clear();
}

void TextureManager::Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
	mDevice = device;
	mContext = context;
}

ID3D11ShaderResourceView* TextureManager::CreateSRV(const std::wstring& filename)
{
	auto i = mSRVs.find(filename);

	if (i != mSRVs.end())
	{
		return i->second;
	}
	else
	{
		std::wstring path = mTextureFolder + filename;

		ID3D11ShaderResourceView* srv;
		ID3D11Resource* resource = nullptr;

		HR(CreateDDSTextureFromFile(mDevice, path.c_str(), &resource, &srv));

		mSRVs.emplace(filename, srv);

		// check created texture
		{
			ID3D11Texture2D* texture = nullptr;
			HR(resource->QueryInterface(IID_ID3D11Texture2D, (void**)&texture));

			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			SafeRelease(texture);
		}

		SafeRelease(resource);

		return srv;
	}
}

ID3D11ShaderResourceView* TextureManager::CreateSRV(const std::vector<std::wstring>& filenames)
{
	std::wstring MapKey;

	for (const std::wstring& filename : filenames)
	{
		MapKey += filename;
	}

	auto i = mSRVs.find(MapKey);

	if (i != mSRVs.end())
	{
		return i->second;
	}
	else
	{
		std::vector<ID3D11Texture2D*> textures(filenames.size(), nullptr);

		for (UINT i = 0; i < filenames.size(); ++i)
		{
			const std::wstring& filename = filenames.at(i);

			auto I = mSRVs.find(filename);

			if (I != mSRVs.end())
			{
				HR(I->second->QueryInterface(IID_ID3D11Texture2D, (void**)&textures.at(i)));
			}
			else
			{
				std::wstring path = mTextureFolder + filename;
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


		} // for each filename

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

		ID3D11ShaderResourceView* srv;

		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = TextureArrayDesc.Format;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MostDetailedMip = 0;
		desc.Texture2DArray.MipLevels = TextureArrayDesc.MipLevels;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = textures.size();

		HR(mDevice->CreateShaderResourceView(TextureArray, &desc, &srv));
		mSRVs.emplace(MapKey, srv);

		SafeRelease(TextureArray);

		for (ID3D11Texture2D* texture : textures)
		{
			SafeRelease(texture);
		}

		return srv;
	}
}

bool Model3DLoader::load(const std::string& filename,
						 std::vector<GeometryGenerator::Vertex>& vertices,
						 std::vector<UINT>& indices,
						 std::vector<Subset>& subsets,
						 std::vector<Model3DMaterial>& materials,
						 SkinnedObject* SkinnedData)
{
	std::string base = "C:/Users/D3PO/source/repos/3D Game Programming with DirectX 11/models/";
	std::ifstream ifs(base + filename);

	UINT nMaterials = 0;
	UINT nVertices = 0;
	UINT nTriangles = 0;
	UINT nBones = 0;
	UINT nAnimationClips = 0;

	std::string ignore;

	if (ifs)
	{
		ifs >> ignore; // ignore header
		ifs >> ignore >> nMaterials;
		ifs >> ignore >> nVertices;
		ifs >> ignore >> nTriangles;
		ifs >> ignore >> nBones;
		ifs >> ignore >> nAnimationClips;

		LoadMaterials(ifs, nMaterials, materials);
		LoadSubsets(ifs, nMaterials, subsets);
		LoadVertices(ifs, nVertices, vertices, SkinnedData ? true : false);
		LoadTriangles(ifs, nTriangles, indices);

		if (SkinnedData)
		{
			LoadBoneOffsets(ifs, nBones, SkinnedData->mBoneOffsets);
			LoadBoneHierarchy(ifs, nBones, SkinnedData->mBoneHierarchy);
			LoadAnimationClips(ifs, nBones, nAnimationClips, SkinnedData->mAnimationClips);
		}

		return true;
	}

	return false;
}

bool Model3DLoader::load(const std::string& filename,
						 TextureManager& manager,
						 GameObject& obj)
{
	std::vector<Model3DMaterial> materials;

	if (load(filename, obj.mMesh.mVertices, obj.mMesh.mIndices, obj.mSubsets, materials, obj.mIsSkinned ? &obj.mSkinnedData : nullptr))
	{
		for (const Model3DMaterial& material : materials)
		{
			obj.mMaterials.push_back(material.material);

			ID3D11ShaderResourceView* DiffuseMapSRV = manager.CreateSRV(material.DiffuseMapFileName);
			obj.mDiffuseMapSRVs.push_back(DiffuseMapSRV);

			ID3D11ShaderResourceView* NormalMapSRV = manager.CreateSRV(material.NormalMapFileName);
			obj.mNormalMapSRVs.push_back(NormalMapSRV);

			obj.mIsAlphaClipping.push_back(material.IsAlphaClipping);
		}
		return true;
	}
	return false;
}

void Model3DLoader::LoadVertices(std::ifstream& ifs,
								 UINT count,
								 std::vector<GeometryGenerator::Vertex>& vertices,
								 bool skinned)
{
	vertices.clear();
	vertices.resize(count);

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		ifs >> ignore >> vertices[i].mPosition.x >> vertices[i].mPosition.y >> vertices[i].mPosition.z;
		ifs >> ignore >> vertices[i].mTangent.x >> vertices[i].mTangent.y >> vertices[i].mTangent.z >> ignore; // vertices[i].mTangent.w;
		ifs >> ignore >> vertices[i].mNormal.x >> vertices[i].mNormal.y >> vertices[i].mNormal.z;
		ifs >> ignore >> vertices[i].mTexCoord.x >> vertices[i].mTexCoord.y;

		if (skinned)
		{
			float weights[4];
			ifs >> ignore >> weights[0] >> weights[1] >> weights[2] >> weights[3];
			vertices[i].mWeights.x = weights[0];
			vertices[i].mWeights.y = weights[1];
			vertices[i].mWeights.z = weights[2];

			UINT BoneIndices[4];
			ifs >> ignore >> BoneIndices[0] >> BoneIndices[1] >> BoneIndices[2] >> BoneIndices[3];
			vertices[i].mBoneIndices[0] = (BYTE)BoneIndices[0];
			vertices[i].mBoneIndices[1] = (BYTE)BoneIndices[1];
			vertices[i].mBoneIndices[2] = (BYTE)BoneIndices[2];
			vertices[i].mBoneIndices[3] = (BYTE)BoneIndices[3];
		}
	}
}

void Model3DLoader::LoadTriangles(std::ifstream& ifs, UINT count, std::vector<UINT>& indices)
{
	indices.clear();
	indices.resize(count * 3);

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		ifs >> indices[i * 3 + 0];
		ifs >> indices[i * 3 + 1];
		ifs >> indices[i * 3 + 2];
	}
}

void Model3DLoader::LoadSubsets(std::ifstream& ifs, UINT count, std::vector<Subset>& subsets)
{
	subsets.clear();
	subsets.resize(count);

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		ifs >> ignore >> subsets[i].id;
		ifs >> ignore >> subsets[i].VertexStart;
		ifs >> ignore >> subsets[i].VertexCount;
		ifs >> ignore >> subsets[i].FaceStart;
		ifs >> ignore >> subsets[i].FaceCount;
	}
}

void Model3DLoader::LoadMaterials(std::ifstream& ifs, UINT count, std::vector<Model3DMaterial>& materials)
{
	materials.clear();
	materials.resize(count);

	std::string DiffuseMapFileName;
	std::string NormalMapFileName;

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		Material& material = materials[i].material;

		ifs >> ignore >> material.mAmbient.x  >> material.mAmbient.y  >> material.mAmbient.z;
		ifs >> ignore >> material.mDiffuse.x  >> material.mDiffuse.y  >> material.mDiffuse.z;
		ifs >> ignore >> material.mSpecular.x >> material.mSpecular.y >> material.mSpecular.z;
		ifs >> ignore >> material.mSpecular.w; // specular factor
		ifs >> ignore >> material.mReflect.x >> material.mReflect.y >> material.mReflect.z;
		ifs >> ignore >> materials[i].IsAlphaClipping;
		ifs >> ignore >> materials[i].EffectName;
		ifs >> ignore >> DiffuseMapFileName;
		ifs >> ignore >> NormalMapFileName;

		materials[i].DiffuseMapFileName.resize(DiffuseMapFileName.size(), ' ');
		std::copy(DiffuseMapFileName.begin(), DiffuseMapFileName.end(), materials[i].DiffuseMapFileName.begin());

		materials[i].NormalMapFileName.resize(NormalMapFileName.size(), ' ');
		std::copy(NormalMapFileName.begin(), NormalMapFileName.end(), materials[i].NormalMapFileName.begin());
	}
}

void Model3DLoader::LoadBoneOffsets(std::ifstream& ifs, UINT count, std::vector<XMFLOAT4X4>& BoneOffsets)
{
	BoneOffsets.clear();
	BoneOffsets.resize(count);

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		ifs >> ignore >>
			BoneOffsets[i](0, 0) >> BoneOffsets[i](0, 1) >> BoneOffsets[i](0, 2) >> BoneOffsets[i](0, 3) >>
			BoneOffsets[i](1, 0) >> BoneOffsets[i](1, 1) >> BoneOffsets[i](1, 2) >> BoneOffsets[i](1, 3) >>
			BoneOffsets[i](2, 0) >> BoneOffsets[i](2, 1) >> BoneOffsets[i](2, 2) >> BoneOffsets[i](2, 3) >>
			BoneOffsets[i](3, 0) >> BoneOffsets[i](3, 1) >> BoneOffsets[i](3, 2) >> BoneOffsets[i](3, 3);
	}
}

void Model3DLoader::LoadBoneHierarchy(std::ifstream& ifs, UINT count, std::vector<UINT>& BoneHierarchy)
{
	BoneHierarchy.clear();
	BoneHierarchy.resize(count);

	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT i = 0; i < count; ++i)
	{
		ifs >> ignore >> BoneHierarchy[i];
	}
}

void Model3DLoader::LoadAnimationClips(std::ifstream& ifs,
									   UINT BoneCount,
									   UINT AnimationClipCount,
									   std::map<std::string, AnimationClip>& animations)
{
	std::string ignore;
	ifs >> ignore; // ignore header

	for (UINT ClipIndex = 0; ClipIndex < AnimationClipCount; ++ClipIndex)
	{
		std::string ClipName;
		ifs >> ignore >> ClipName;
		ifs >> ignore; // {

		AnimationClip clip;
		clip.mAnimationObjects.resize(BoneCount);

		for (UINT BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			LoadAnimation(ifs, BoneCount, clip.mAnimationObjects[BoneIndex]);
		}
		ifs >> ignore; // }

		animations[ClipName] = clip;
	}
}

void Model3DLoader::LoadAnimation(std::ifstream& ifs, UINT BoneCount, AnimationObject& animation)
{
	UINT keyframes = 0;

	std::string ignore;
	ifs >> ignore >> ignore >> keyframes;
	ifs >> ignore; // {

	animation.keyframes.resize(keyframes);

	for (UINT i = 0; i < keyframes; ++i)
	{
		float t = 0.0f;
		XMFLOAT3 T(0.0f, 0.0f, 0.0f);
		XMFLOAT3 S(1.0f, 1.0f, 1.0f);
		XMFLOAT4 R(0.0f, 0.0f, 0.0f, 1.0f);

		ifs >> ignore >> t;
		ifs >> ignore >> T.x >> T.y >> T.z;
		ifs >> ignore >> S.x >> S.y >> S.z;
		ifs >> ignore >> R.x >> R.y >> R.z >> R.w;

		animation.keyframes[i].time = t;
		animation.keyframes[i].translation = T;
		animation.keyframes[i].scale = S;
		animation.keyframes[i].rotation = R;
	}

	ifs >> ignore; // }
}

float SkinnedObject::GetTimeClipStart(const std::string& ClipName)
{
	return mAnimationClips.at(ClipName).GetTimeClipStart();
}

float SkinnedObject::GetTimeClipEnd(const std::string& ClipName)
{
	return mAnimationClips.at(ClipName).GetTimeClipEnd();
}

void SkinnedObject::GetTransforms(const std::string& animation,
								  float t,
								  std::vector<XMFLOAT4X4>& transforms)
{
	UINT BoneCount = mBoneOffsets.size();

	std::vector<XMMATRIX> ToParentTransforms(BoneCount);
	std::vector<XMMATRIX> ToRootTransforms(BoneCount);

	// interpolate all the bones of this clip at t
	mAnimationClips.at(animation).interpolate(t, ToParentTransforms);

	// traverse the hierarchy and ...

	// the root bone has index 0 and has no parent,
	// so its ToRootTransform is just its local bone transform
	ToRootTransforms[0] = ToParentTransforms[0];

	// find the ToRootTransform of the children
	for (UINT i = 1; i < BoneCount; ++i)
	{
		XMMATRIX ToParent = ToParentTransforms[i];

		// parent's ToRoot
		UINT ParentIndex = mBoneHierarchy[i];
		XMMATRIX ToRoot = ToRootTransforms[ParentIndex];

		ToRootTransforms[i] = ToParent * ToRoot;
	}

	// ... and transform all the bones to the root space
	for (UINT i = 0; i < BoneCount; ++i)
	{
		// premultiply by the bone offset transform to get the final transform.
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		XMMATRIX ToRoot = ToRootTransforms[i];
		XMStoreFloat4x4(&transforms[i], offset * ToRoot);
	}

}

void GameObject::LoadModel(ID3D11Device* device, TextureManager& manager, const std::string& filename, bool skinned)
{
	mIsSkinned = skinned;
	Model3DLoader().load(filename, manager, *this);
}

void GameObjectInstance::update(float dt)
{
	time += dt;
	obj->mSkinnedData.GetTransforms(ClipName, time, transforms);

	if (time > obj->mSkinnedData.GetTimeClipEnd(ClipName))
	{
		time = 0; // loop animation
	}
}