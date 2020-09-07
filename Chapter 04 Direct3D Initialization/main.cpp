#include <D3DApp/D3DApp.h>
#include <array>
#include <cassert>

class TestApp : public D3DApp
{
public:
	TestApp();
	~TestApp();

	bool Init();
	void UpdateScene(float dt);
	void DrawScene();
};

TestApp::TestApp()
	: D3DApp()
{}

TestApp::~TestApp()
{}

bool TestApp::Init()
{
	return D3DApp::Init();
}

void TestApp::UpdateScene(float dt)
{}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	std::array<FLOAT, 4> red    = { 1.0f, 0.0f, 0.0f, 1.0f };
	std::array<FLOAT, 4> blue   = { 0.0f, 0.0f, 1.0f, 1.0f };
	std::array<FLOAT, 4> yellow = { 1.0f, 1.0f, 0.0f, 1.0f };

	mContext->ClearRenderTargetView(mRenderTargetView, red.data());
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}