#ifndef D3DAPP_H
#define D3DAPP_H

#define NOMINMAX

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <array>

#include <glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include <d3dcompiler.h>

#include <DDSTextureLoader.h>

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

class CameraObject
{
public:
	// camera
	XMFLOAT3 mPosition;
	XMFLOAT3 mRight;
	XMFLOAT3 mUp;
	XMFLOAT3 mLook;

	// frustum
	float mNearZ;
	float mFarZ;
	float mAspectRatio;
	float mFovAngleY;
	float mNearWindowHeight;
	float mFarWindowHeight;

	BoundingFrustum mFrustum;

	// view/proj
	// XMMATRIX mView;
	XMFLOAT4X4 mView;
	XMMATRIX mProj;

	CameraObject();

	void SetFrustum(float FovAngleY, float AspectRatio, float NearZ, float FarZ);

	void walk(float delta);
	void strafe(float delta);

	void LookAt(const XMFLOAT3& position, const XMFLOAT3& target, const XMFLOAT3& up);
	void UpdateView();

	void pitch(float angle);
	void rotate(float angle);
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
	virtual void OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods);
	bool IsKeyPressed(int key) { return mKeysState.at(key); }
	virtual void UpdateScene(float dt) = 0;
	virtual void DrawScene() = 0;

	float AspectRatio() const;

	void CreateSRV(const std::wstring& name, ID3D11ShaderResourceView** view);

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
	// rasterizer states
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mWireframeRS;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mNoCullRS;
	// depth stencil states
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mLessEqualDSS;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mEqualDSS;

	// window stuff
	GLFWwindow* mMainWindow;
	int mMainWindowWidth;
	int mMainWindowHeight;
	std::string mMainWindowTitle;
	bool mResizing;
	std::array<bool, GLFW_KEY_LAST> mKeysState;

	// mouse events
	float mTheta;
	float mPhi;
	float mRadius;
	XMFLOAT2 mLastMousePos;

	CameraObject mCamera;
};

class GeometryGenerator
{
public:
	struct Vertex
	{
		XMFLOAT3 mPosition;
		XMFLOAT3 mNormal;
		XMFLOAT3 mTangent;
		//XMFLOAT4 mColor;
		XMFLOAT2 mTexCoord;

		Vertex() {}

		Vertex(float Px, float Py, float Pz,
			   float Nx, float Ny, float Nz,
			   float Tx, float Ty, float Tz,
			   float Tu, float Tv) :
			mPosition(Px, Py, Pz),
			mNormal(Nx, Ny, Nz),
			mTangent(Tx, Ty, Tz),
			mTexCoord(Tu, Tv)
		{}

		//Vertex(float Px, float Py, float Pz,
		//	   float Nx, float Ny, float Nz,
		//	   float Cr, float Cg, float Cb, // float Cr, float Cg, float Cb, float Ca,
		//	   float Tu, float Tv) :
		//	mPosition(Px, Py, Pz),
		//	mNormal(Nx, Ny, Nz),
		//	mColor(Cr, Cb, Cg, 1), // mColor(Cr, Cb, Cg, Ca),
		//	mTexCoord(Tu, Tv)
		//{}
	};

	struct Mesh
	{
		std::vector<Vertex> mVertices;
		std::vector<UINT> mIndices;

		BoundingBox mAABB;

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
	static void CreateSphere(float radius, UINT slices, UINT stacks, Mesh& mesh);

	static void CreateModel(std::string name, Mesh& mesh);
	static void CreateSkull(Mesh& mesh) { CreateModel("skull.txt", mesh); };
	static void CreateCar(Mesh& mesh) { CreateModel("car.txt", mesh); };

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
	
	// random float in [0, 1)
	static float RandNorm() { return (float)rand() / (float)RAND_MAX; }

	// random float in [a, b)
	static float RandNorm(float a, float b) { return a + (b - a) * RandNorm(); }

	static XMMATRIX InverseTranspose(XMMATRIX M);
};

class GameObject
{
public:
	GeometryGenerator::Mesh mMesh;

	Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer;

	UINT mVertexStart;
	UINT mIndexStart;
	UINT mIndexCount;

	XMMATRIX mWorld;
	XMMATRIX mTexCoordTransform;

	struct Material
	{
		Material() { ZeroMemory(this, sizeof(this)); }

		XMFLOAT4 mAmbient;
		XMFLOAT4 mDiffuse;
		XMFLOAT4 mSpecular;
		XMFLOAT4 mReflect;
	};

	Material mMaterial;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mAlbedoSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mNormalSRV;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> mHullShader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> mDomainShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> mGeometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  mPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout;
	D3D11_PRIMITIVE_TOPOLOGY mPrimitiveTopology;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> mRasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> mBlendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState;
	UINT mStencilRef;

	struct InstancedData
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4   mColor;
	};

	ID3D11Buffer* mInstancedBuffer;
	std::vector<InstancedData> mInstances;
	
	UINT mVisibleInstanceCount;

	GameObject() :
		mVertexBuffer(nullptr),
		mIndexBuffer(nullptr),
		mInstancedBuffer(nullptr),
		mVertexStart(0),
		mIndexStart(0),
		mIndexCount(0),
		mWorld(XMMatrixIdentity()),
		mTexCoordTransform(XMMatrixIdentity()),
		mAlbedoSRV(nullptr),
		mNormalSRV(nullptr),
		mVertexShader(nullptr),
		mHullShader(nullptr),
		mDomainShader(nullptr),
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
		SafeRelease(mInstancedBuffer);
		SafeRelease((*mAlbedoSRV.GetAddressOf()));
		SafeRelease((*mNormalSRV.GetAddressOf()));
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

class DynamicCubeMap
{
public:
	ID3D11DepthStencilView* mDSV;
	ID3D11RenderTargetView* mRTV[6];
	ID3D11ShaderResourceView* mSRV;
	
	UINT mCubeMapSize;
	D3D11_VIEWPORT mViewport;

	CameraObject mCamera[6];

	DynamicCubeMap();
	~DynamicCubeMap();

	void Init(ID3D11Device* device, const XMFLOAT3& P);
	//void Draw();
};

class DebugQuad : public GameObject
{
public:
	enum ScreenCorner
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	DebugQuad();
	~DebugQuad();

	void Init(ID3D11Device* device, float WindowAspectRatio, ScreenCorner position, float TextureAspectRatio);
	void OnResize(float AspectRatio);
	void Draw(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv);

private:
	ScreenCorner mPosition;
	float mAspectRatio;

	ID3D11Buffer* mDebugQuadCB;

	struct DebugQuadCB
	{
		XMFLOAT4X4 mWorldViewProj;
	};
};

class ShadowMap
{
	UINT mWidth;
	UINT mHeight;

	ID3D11DepthStencilView* mDSV;
	ID3D11ShaderResourceView* mSRV;

	D3D11_VIEWPORT mViewport;

	ID3D11Buffer* mPerObjectCB;

	ID3D11VertexShader* mVertexShader;
	ID3D11InputLayout* mInputLayout;

	ID3D11RasterizerState* mRasterizerState;
	
	ID3D11SamplerState* mSamplerState;

public:
	ShadowMap();
	~ShadowMap();

	void Init(ID3D11Device* device, UINT width, UINT height);

	void BindDSVAndSetNullRenderTarget(ID3D11DeviceContext* context);

	XMFLOAT4X4 mLightView;
	XMFLOAT4X4 mLightProj;
	XMFLOAT4X4 mShadowTransform;

	void BuildTranform(const XMFLOAT3& light, const BoundingSphere& bounds);

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorldViewProj;
		XMFLOAT4X4 mTexTransform;
	};

	ID3D11VertexShader* GetVS();
	ID3D11InputLayout* GetIL();
	ID3D11RasterizerState* GetRS();
	ID3D11SamplerState*& GetSS(); // return reference to pointer
	ID3D11ShaderResourceView*& GetSRV();
	ID3D11Buffer*& GetCB();

	DebugQuad mDebugQuad;
};

class SSAO
{
	UINT mWidth;
	UINT mHeight;

	ID3D11RenderTargetView* mNormalDepthRTV;
	ID3D11ShaderResourceView* mNormalDepthSRV;
	ID3D11VertexShader* mNormalDepthVS;
	ID3D11InputLayout* mNormalDepthIL;
	ID3D11PixelShader* mNormalDepthPS;
	ID3D11Buffer* mNormalDepthCB;
	ID3D11SamplerState* mNormalDepthSS;

	ID3D11ShaderResourceView* mRandomVectorSRV;
	ID3D11SamplerState* mRandomVectorSS;
	XMFLOAT4 mFrustumFarCorner[4];
	XMFLOAT4 mSampleOffset[14];

	ID3D11RenderTargetView* mAmbientMapRTV[2];
	ID3D11ShaderResourceView* mAmbientMapSRV[2];
	D3D11_VIEWPORT mAmbientMapViewport;
	GameObject mAmbientMapQuad;
	ID3D11Buffer* mAmbientMapComputeCB;

	ID3D11VertexShader* mBlurVS;
	ID3D11InputLayout* mBlurIL;
	ID3D11PixelShader* mBlurPS[2];
	ID3D11Buffer* mBlurCB;
	ID3D11SamplerState* mBlurSS;

	struct BlurCB
	{
		float TexelWidth;
		float TexelHeight;
		XMFLOAT2 padding;
	};

	static_assert((sizeof(BlurCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	void BlurAmbientMap(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv, ID3D11ShaderResourceView* srv, bool HorizontalBlur);

public:
	SSAO();
	~SSAO();

	void Init(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float FarZ);
	void OnResize(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float FarZ);

	void BindNormalDepthRenderTarget(ID3D11DeviceContext* context, ID3D11DepthStencilView* dsv);

	void ComputeAmbientMap(ID3D11DeviceContext* context, const CameraObject& camera);
	void BlurAmbientMap(ID3D11DeviceContext* context, UINT count);

	struct NormalDepthCB
	{
		XMFLOAT4X4 WorldView;
		XMFLOAT4X4 WorldViewProj;
		XMFLOAT4X4 WorldInverseTransposeView;
		XMFLOAT4X4 TexCoordTransform;
	};

	static_assert((sizeof(NormalDepthCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	struct AmbientMapComputeCB
	{
		XMFLOAT4X4 ProjTexture;
		XMFLOAT4 SampleOffset[14];
		XMFLOAT4 FrustumFarCorner[4];

		// coordinates given in view space
		float OcclusionRadius;
		float OcclusionFadeStart;
		float OcclusionFadeEnd;
		float SurfaceEpsilon;
	};

	static_assert((sizeof(AmbientMapComputeCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer*& GetNormalDepthCB();
	ID3D11VertexShader* GetNormalDepthVS();
	ID3D11InputLayout* GetNormalDepthIL();
	ID3D11PixelShader* GetNormalDepthPS();

	ID3D11ShaderResourceView*& GetAmbientMapSRV();
	ID3D11RenderTargetView*& GetAmbientMapRTV();

	DebugQuad mDebugQuad;
};

#endif // D3DAPP_H