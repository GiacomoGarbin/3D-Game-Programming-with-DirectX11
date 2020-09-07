#include "D3DApp.h"

#include <sstream>
#include <cassert>
#include <vector>

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
	mRadius(10),
	mLastMousePos(0, 0)
{
	ZeroMemory(&mViewport, sizeof(D3D11_VIEWPORT));
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
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		float dx = 0.25f * XMConvertToRadians(xpos - mLastMousePos.x);
		float dy = 0.25f * XMConvertToRadians(ypos - mLastMousePos.y);

		mTheta += dx;
		mPhi += dy;

		mPhi = std::clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
	{
		float dx = 0.5f * XMConvertToRadians(xpos - mLastMousePos.x);
		float dy = 0.5f * XMConvertToRadians(ypos - mLastMousePos.y);

		mRadius += (dx - dy);

		mRadius = std::clamp(mRadius, 3.f, 150.f);
	}

	mLastMousePos = XMFLOAT2(xpos, ypos);
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

	//// front
	mesh.mVertices.at(0x00) = Vertex(-w, -h, -d, 0, 0, -1, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x01) = Vertex(-w, +h, -d, 0, 0, -1, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x02) = Vertex(+w, +h, -d, 0, 0, -1, 0, 0, 0, 1, 0);
	mesh.mVertices.at(0x03) = Vertex(+w, -h, -d, 0, 0, -1, 0, 0, 0, 1, 1);

	//// back
	mesh.mVertices.at(0x04) = Vertex(-w, -h, +d, 0, 0, +1, 0, 0, 0, 1, 1);
	mesh.mVertices.at(0x05) = Vertex(+w, -h, +d, 0, 0, +1, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x06) = Vertex(+w, +h, +d, 0, 0, +1, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x07) = Vertex(-w, +h, +d, 0, 0, +1, 0, 0, 0, 1, 0);

	//// top
	mesh.mVertices.at(0x08) = Vertex(-w, +h, -d, 0, +1, 0, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x09) = Vertex(-w, +h, +d, 0, +1, 0, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x0A) = Vertex(+w, +h, +d, 0, +1, 0, 0, 0, 0, 1, 0);
	mesh.mVertices.at(0x0B) = Vertex(+w, +h, -d, 0, +1, 0, 0, 0, 0, 1, 1);

	//// bottom
	mesh.mVertices.at(0x0C) = Vertex(-w, -h, -d, 0, -1, 0, 0, 0, 0, 1, 1);
	mesh.mVertices.at(0x0D) = Vertex(+w, -h, -d, 0, -1, 0, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x0E) = Vertex(+w, -h, +d, 0, -1, 0, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x0F) = Vertex(-w, -h, +d, 0, -1, 0, 0, 0, 0, 1, 0);

	//// left
	mesh.mVertices.at(0x10) = Vertex(-w, -h, +d, -1, 0, 0, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x11) = Vertex(-w, +h, +d, -1, 0, 0, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x12) = Vertex(-w, +h, -d, -1, 0, 0, 0, 0, 0, 1, 0);
	mesh.mVertices.at(0x13) = Vertex(-w, -h, -d, -1, 0, 0, 0, 0, 0, 1, 1);

	//// right
	mesh.mVertices.at(0x14) = Vertex(+w, -h, -d, +1, 0, 0, 0, 0, 0, 0, 1);
	mesh.mVertices.at(0x15) = Vertex(+w, +h, -d, +1, 0, 0, 0, 0, 0, 0, 0);
	mesh.mVertices.at(0x16) = Vertex(+w, +h, +d, +1, 0, 0, 0, 0, 0, 1, 0);
	mesh.mVertices.at(0x17) = Vertex(+w, -h, +d, +1, 0, 0, 0, 0, 0, 1, 1);

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
			mesh.mVertices.at(k).mColor    = XMFLOAT4((x + HalfWidth) / width, 0, (z + HalfDepth) / depth, 1);
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

			XMVECTOR T = XMLoadFloat3(&XMFLOAT3(-s, 0, c));
			XMVECTOR B = XMLoadFloat3(&XMFLOAT3((dr * stacks) * c, -height, (dr * stacks) * s));
			XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));

			XMStoreFloat3(&vertex.mNormal, N);

			vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

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
			vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);
			vertex.mTexCoord = XMFLOAT2(u, v);

			mesh.mVertices.push_back(vertex);
		}

		Vertex vertex;
		vertex.mPosition = XMFLOAT3(0, y, 0);
		vertex.mNormal = XMFLOAT3(0, type ? +1 : -1, 0);
		vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);
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

		vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

		float t = GameMath::GetAngle2(XMFLOAT2(vertex.mPosition.x, vertex.mPosition.z)); // theta
		float p = std::acos(vertex.mPosition.y / radius); // phi

		vertex.mTexCoord = XMFLOAT2(t / XM_2PI, p / XM_2PI);
	}
}

void GeometryGenerator::CreateModel(std::string name, Mesh& mesh)
{
	mesh.mVertices.clear();
	mesh.mIndices.clear();

	std::ifstream ifs;

	ifs.open("C:/Users/D3PO/source/repos/DirectX11Tutorial/models/" + name);

	std::string line;

	bool vertices = false;
	bool indices = false;

	while (std::getline(ifs, line))
	{
		if (vertices)
		{
			std::stringstream ss(line);
			GeometryGenerator::Vertex vertex;

			ss >> vertex.mPosition.x >> vertex.mPosition.y >> vertex.mPosition.z;
			ss >> vertex.mNormal.x >> vertex.mNormal.y >> vertex.mNormal.z;

			vertex.mColor = XMFLOAT4((vertex.mNormal.x + 1) * 0.5f, (vertex.mNormal.y + 1) * 0.5f, (vertex.mNormal.z + 1) * 0.5f, 1);

			mesh.mVertices.push_back(vertex);
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

	// mesh.ComputeNormals();
}

void GeometryGenerator::CreateScreenQuad(Mesh& mesh)
{
	auto& v = mesh.mVertices;
	auto& i = mesh.mIndices;

	v.clear();
	i.clear();

	v.resize(4);

	v.at(0) = Vertex(-1, -1, 0,   0, 0, -1,   0, 1);
	v.at(1) = Vertex(-1, +1, 0,   0, 0, -1,   0, 0);
	v.at(2) = Vertex(+1, +1, 0,   0, 0, -1,   1, 0);
	v.at(3) = Vertex(+1, -1, 0,   0, 0, -1,   1, 1);

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

	if (P.x == 0 && P.y != 0)
	{
		theta = P.y > 0 ? +XM_PIDIV2 : -XM_PIDIV2;
	}
	else if (P.x > 0) // Quadrant I or IV
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