#ifndef D3DAPP_H
#define D3DAPP_H

#define NOMINMAX

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>

#include <glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>

#include <D3d11.h>
#include <directxmath.h>
#include <DirectXColors.h>
using namespace DirectX;

#include <wrl/client.h>
//using namespace Microsoft::WRL;

#define SafeRelease(obj) { if (obj) { obj->Release(); obj = nullptr; } }

#ifdef _DEBUG
#ifndef HR
#define HR(exp)																								\
{																											\
	HRESULT hr = exp;																						\
	if (FAILED(hr))																							\
	{																										\
		LPCSTR output;																						\
		FormatMessage																						\
		(																									\
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,	\
			NULL,																							\
			hr,																								\
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),														\
			(LPTSTR)&output,																				\
			0,																								\
			NULL																							\
		);																									\
		MessageBox(NULL, output, "Error", MB_OK);															\
	}																										\
}
#endif // HR
#else // _DEBUG
#ifndef HR
#define HR(exp) exp
#endif // HR
#endif // _DEBUG

class GameTimer
{
	double mSecondsPerCount;
	double mDeltaTime;
	__int64 mBaseTime;
	__int64 mPauseTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;
	bool mStopped;

public:
	GameTimer();

	float TotalTime() const;
	float DeltaTime() const;

	void Reset();
	void Start();
	void Stop();
	void Tick();
};

class D3DApp
{
public:
	D3DApp();
	virtual ~D3DApp();

	int Run();

	virtual bool Init();
	virtual void OnResize(GLFWwindow* window, int width, int height);
	virtual void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
	virtual void OnMouseMove(GLFWwindow* window, double xpos, double ypos);
	virtual void UpdateScene(float dt) = 0;
	virtual void DrawScene() = 0;

	float AspectRatio() const;

protected:
	bool InitMainWindow();
	bool InitDirect3D();

	void CalculateFrameStats();

	GameTimer mTimer;
	bool mAppPaused;

	// D3D11 stuff
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mContext;
	bool m4xMSAAEnabled;
	UINT m4xMSAAQuality;
	IDXGISwapChain* mSwapChain;
	ID3D11Texture2D* mDepthStencilBuffer;
	ID3D11RenderTargetView* mRenderTargetView;
	ID3D11DepthStencilView* mDepthStencilView;
	D3D11_VIEWPORT mViewport;

	// window stuff
	GLFWwindow* mMainWindow;
	int mMainWindowWidth;
	int mMainWindowHeight;
	std::string mMainWindowTitle;
	bool mResizing;

	// mouse events
	float mTheta;
	float mPhi;
	float mRadius;
	XMFLOAT2 mLastMousePos;
};

class GeometryGenerator
{
public:
	struct Vertex
	{
		XMFLOAT3 mPosition;
		XMFLOAT3 mNormal;
		XMFLOAT4 mColor;
		XMFLOAT2 mTexCoord;

		Vertex() {}

		Vertex(float Px, float Py, float Pz,
			   float Nx, float Ny, float Nz,
			   float Tu, float Tv) :
			mPosition(Px, Py, Pz),
			mNormal(Nx, Ny, Nz),
			mTexCoord(Tu, Tv)
		{}

		Vertex(float Px, float Py, float Pz,
			   float Nx, float Ny, float Nz,
			   float Cr, float Cg, float Cb, // float Cr, float Cg, float Cb, float Ca,
			   float Tu, float Tv) :
			mPosition(Px, Py, Pz),
			mNormal(Nx, Ny, Nz),
			mColor(Cr, Cb, Cg, 1), // mColor(Cr, Cb, Cg, Ca),
			mTexCoord(Tu, Tv)
		{}
	};

	struct Mesh
	{
		std::vector<Vertex> mVertices;
		std::vector<UINT> mIndices;

		void ComputeNormals() { ComputeNormals(mVertices, mIndices); };
		static void ComputeNormals(
			std::vector<Vertex>& vertices,
			std::vector<UINT>& indices)
		{ ComputeNormals(vertices.begin(), vertices.end(), indices.begin(), indices.end()); };
		static void ComputeNormals(
			std::vector<Vertex>::iterator FirstVertex,
			std::vector<Vertex>::iterator LastVertex,
			std::vector<UINT>::iterator FirstIndex,
			std::vector<UINT>::iterator LastIndex);
	};

	static void CreateBox(float width, float height, float depth, Mesh& mesh);
	static void CreateGrid(float width, float depth, UINT m, UINT n, Mesh& mesh);
	static void CreateCylinder(float BottomRadius, float TopRadius, float height, UINT slices, UINT stacks, Mesh& mesh);
	static void CreateSphere(float radius, UINT n, Mesh& mesh);

	static void CreateModel(std::string name, Mesh& mesh);
	static void CreateSkull(Mesh& mesh) { CreateModel("skull.txt", mesh); };

	static void CreateScreenQuad(Mesh& mesh);

	class Waves
	{
	public:
		Waves();
		~Waves();

		void Init(UINT m, UINT n, float dx, float dt, float speed, float damping);
		void Update(float dt);
		void Disturb(UINT i, UINT j, float magnitude);

		// void CreateIndices(std::vector<UINT>& indices);

		const Vertex& operator[](int i) const { return mCurrVertices.at(i); }

		UINT mRows;
		UINT mCols;

		float mK1, mK2, mK3;

		float mTimeStep;
		float mSpaceStep;

		std::vector<Vertex> mPrevVertices;
		std::vector<Vertex> mCurrVertices;
	};
};

class GameMath
{
public:
	static float GetAngle2(XMFLOAT2 P);
	static float RandNorm() { return (float)rand() / (float)RAND_MAX; }
	static float RandNorm(float a, float b) { return a + (b - a) * RandNorm(); }

	static XMMATRIX InverseTranspose(XMMATRIX M);
};

class GameObject
{
public:
	GeometryGenerator::Mesh mMesh;

	//ID3D11Buffer* mVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer;
	//ID3D11Buffer* mIndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer;

	UINT mVertexStart;
	UINT mIndexStart;
	UINT mIndexCount;

	XMMATRIX mWorld;
	XMMATRIX mTexTransform;

	struct Material
	{
		Material() { ZeroMemory(this, sizeof(this)); }

		XMFLOAT4 mAmbient;
		XMFLOAT4 mDiffuse;
		XMFLOAT4 mSpecular;
		XMFLOAT4 mReflect;
	};

	Material mMaterial;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> mGeometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  mPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout;
	D3D11_PRIMITIVE_TOPOLOGY mPrimitiveTopology;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mRasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mBlendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState;
	UINT mStencilRef;

	GameObject() :
		mVertexBuffer(nullptr),
		mIndexBuffer(nullptr),
		mVertexStart(0),
		mIndexStart(0),
		mIndexCount(0),
		mWorld(XMMatrixIdentity()),
		mTexTransform(XMMatrixIdentity()),
		mSRV(nullptr),
		mVertexShader(nullptr),
		mGeometryShader(nullptr),
		mPixelShader(nullptr),
		mInputLayout(nullptr),
		mPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
		mRasterizerState(nullptr),
		mBlendState(nullptr),
		mDepthStencilState(nullptr),
		mStencilRef(0)
	{}

	~GameObject()
	{
		SafeRelease((*mVertexBuffer.GetAddressOf()));
		SafeRelease((*mIndexBuffer.GetAddressOf()));
		SafeRelease((*mSRV.GetAddressOf()));
		SafeRelease((*mVertexShader.GetAddressOf()));
		SafeRelease((*mGeometryShader.GetAddressOf()));
		SafeRelease((*mPixelShader.GetAddressOf()));
		SafeRelease((*mInputLayout.GetAddressOf()));
		SafeRelease((*mRasterizerState.GetAddressOf()));
		SafeRelease((*mBlendState.GetAddressOf()));
		SafeRelease((*mDepthStencilState.GetAddressOf()));
	}
};


struct LightDirectional
{
	LightDirectional() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 mAmbient;
	XMFLOAT4 mDiffuse;
	XMFLOAT4 mSpecular;

	XMFLOAT3 mDirection;
	float pad;
};

struct LightPoint
{
	LightPoint() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 mAmbient;
	XMFLOAT4 mDiffuse;
	XMFLOAT4 mSpecular;

	XMFLOAT3 mPosition;
	float mRange;
	XMFLOAT3 mAttenuation;
	float pad;
};

struct LightSpot
{
	LightSpot() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 mAmbient;
	XMFLOAT4 mDiffuse;
	XMFLOAT4 mSpecular;

	XMFLOAT3 mPosition;
	float mRange;
	XMFLOAT3 mDirection;
	float mCone;
	XMFLOAT3 mAttenuation;
	float pad;
};

class BlurEffect
{
	UINT mWidth;
	UINT mHeight;
	DXGI_FORMAT mFormat;

	ID3D11ShaderResourceView* mOutputTextureSRV;
	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mOutputTextureSRV;
	ID3D11UnorderedAccessView* mOutputTextureUAV;

public:

	ID3D11ComputeShader* mHorBlurCS;
	ID3D11ComputeShader* mVerBlurCS;

	BlurEffect() :
		mWidth(0),
		mHeight(0),
		mFormat(DXGI_FORMAT_UNKNOWN),
		mOutputTextureSRV(nullptr),
		mOutputTextureUAV(nullptr),
		mHorBlurCS(nullptr),
		mVerBlurCS(nullptr)
	{}

	~BlurEffect()
	{
		//SafeRelease((*mOutputTextureSRV.GetAddressOf()));
		SafeRelease(mOutputTextureSRV);
		SafeRelease(mOutputTextureUAV);

		SafeRelease(mHorBlurCS);
		SafeRelease(mVerBlurCS);
	}

	//ID3D11ShaderResourceView* GetBlurredTextureSRV() { return mOutputTextureSRV; }
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetBlurredTextureSRV() { return mOutputTextureSRV; }

	void Init(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format);
	void OnResize(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format);
	void Blur(ID3D11DeviceContext* context, ID3D11ShaderResourceView* InputTextureSRV, ID3D11UnorderedAccessView* InputTextureUAV, UINT count);

	//void SetGaussianWeights(float sigma);
};

#endif // D3DAPP_H