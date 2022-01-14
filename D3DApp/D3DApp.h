#ifndef D3DAPP_H
#define D3DAPP_H

#define NOMINMAX

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <array>
#include <map>

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

#define SafeDelete(obj) { delete obj; obj = nullptr; }
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

class TextureManager
{
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mContext;
	std::map<std::wstring, ID3D11ShaderResourceView*> mSRVs;
	std::vector<ID3D11ShaderResourceView*> mNamelessTextureSRVs;

public:
	TextureManager();
	~TextureManager();

	std::wstring mTextureFolder;

	void init(ID3D11Device* device, ID3D11DeviceContext* context);
	ID3D11ShaderResourceView* CreateSRV(const std::wstring& filename);
	ID3D11ShaderResourceView* CreateSRV(const std::vector<std::wstring>& filenames);

	UINT CreateRandomTexture1DSRV();
	ID3D11ShaderResourceView* GetNamelessTextureSRV(UINT index);
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

	TextureManager mTextureManager;

protected:
	bool InitMainWindow();
	bool InitDirect3D();

	void CalculateFrameStats();

	GameTimer mTimer;
	bool mAppPaused;

	UINT mFrameIndex;

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
		XMFLOAT2 mTexCoord;
		XMFLOAT3 mWeights;
		BYTE	 mBoneIndices[4];

		Vertex() {}

		Vertex(float Px, float Py, float Pz,
			   float Nx, float Ny, float Nz,
			   float Tx, float Ty, float Tz, // float Tw,
			   float Tu, float Tv) :
			mPosition(Px, Py, Pz),
			mNormal(Nx, Ny, Nz),
			mTangent(Tx, Ty, Tz),
			mTexCoord(Tu, Tv)
		{
			ZeroMemory(&mWeights, sizeof(mWeights));
			ZeroMemory(&mBoneIndices, sizeof(mBoneIndices));
		}
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

struct Material
{
	Material() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 mAmbient;
	XMFLOAT4 mDiffuse;
	XMFLOAT4 mSpecular;
	XMFLOAT4 mReflect;
};

class AnimationObject
{
	struct KeyFrame
	{
		float time;
		XMFLOAT3 translation;
		XMFLOAT3 scale;
		XMFLOAT4 rotation; // quaternion

		KeyFrame();
		~KeyFrame();
	};

public:
	// keyframes are sorted by time
	std::vector<KeyFrame> keyframes;

	float mCurrTime;

	AnimationObject() :
		mCurrTime(0)
	{}

	float GetTimeStart() const;
	float GetTimeEnd() const;
	void interpolate(float t, XMMATRIX& world) const;
};

class AnimationClip
{
public:
	// an AnimationObject for every bone
	std::vector<AnimationObject> mAnimationObjects;

	float GetTimeClipStart() const;
	float GetTimeClipEnd() const;
	void interpolate(float t, std::vector<XMMATRIX>& transforms) const;
};

class SkinnedObject
{
public:
	// parent index of ith bone
	std::vector<UINT> mBoneHierarchy;
	std::vector<XMFLOAT4X4> mBoneOffsets;
	std::map<std::string, AnimationClip> mAnimationClips;

	float GetTimeClipStart(const std::string& ClipName);
	float GetTimeClipEnd(const std::string& ClipName);

	void GetTransforms(const std::string& animation,
					   float t,
					   std::vector<XMFLOAT4X4>& transforms);
};

struct Subset
{
	UINT id;
	UINT VertexStart;
	UINT VertexCount;
	UINT FaceStart;
	UINT FaceCount;

	Subset() :
		id(-1),
		VertexStart(0),
		VertexCount(0),
		FaceStart(0),
		FaceCount(0)
	{}
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

	Material mMaterial;

	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mAlbedoSRV;
	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mNormalSRV;
	ID3D11ShaderResourceView* mAlbedoSRV;
	ID3D11ShaderResourceView* mNormalSRV;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> mHullShader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> mDomainShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> mGeometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShader;
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

	AnimationObject mAnimation;

	std::vector<Material> mMaterials;
	std::vector<ID3D11ShaderResourceView*> mDiffuseMapSRVs;
	std::vector<ID3D11ShaderResourceView*> mNormalMapSRVs;
	std::vector<Subset> mSubsets;
	std::vector<bool> mIsAlphaClipping;

	bool mIsSkinned;
	SkinnedObject mSkinnedData;

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
		mStencilRef(0),
		mIsSkinned(false)
	{}

	~GameObject()
	{
		SafeRelease((*mVertexBuffer.GetAddressOf()));
		SafeRelease((*mIndexBuffer.GetAddressOf()));
		SafeRelease(mInstancedBuffer);
		//SafeRelease((*mAlbedoSRV.GetAddressOf()));
		//SafeRelease((*mNormalSRV.GetAddressOf()));
		//SafeRelease(mAlbedoSRV);
		//SafeRelease(mNormalSRV);
		SafeRelease((*mVertexShader.GetAddressOf()));
		SafeRelease((*mGeometryShader.GetAddressOf()));
		SafeRelease((*mPixelShader.GetAddressOf()));
		SafeRelease((*mInputLayout.GetAddressOf()));
		SafeRelease((*mRasterizerState.GetAddressOf()));
		SafeRelease((*mBlendState.GetAddressOf()));
		SafeRelease((*mDepthStencilState.GetAddressOf()));
	}

	void LoadModel(ID3D11Device* device, TextureManager& manager, const std::string& filename, bool skinned = false);
};

struct GameObjectInstance
{
	GameObject* obj;
	XMMATRIX world;

	float time;
	std::string ClipName;
	std::vector<XMFLOAT4X4> transforms;

	GameObjectInstance() :
		obj(nullptr),
		world(XMMatrixIdentity()),
		time(0)
	{}

	void update(float dt);
};

struct Model3DMaterial
{
	Material material;
	bool IsAlphaClipping;
	std::string EffectName;
	std::wstring DiffuseMapFileName;
	std::wstring NormalMapFileName;
};

class Model3DLoader
{
public:
	bool load(const std::string& filename,
			  std::vector<GeometryGenerator::Vertex>& vertices,
			  std::vector<UINT>& indices,
			  std::vector<Subset>& subsets,
			  std::vector<Model3DMaterial>& materials,
			  SkinnedObject* SkinnedData = nullptr);

	bool load(const std::string& filename,
			  TextureManager& manager,
			  GameObject& obj);

private:
	void LoadVertices(std::ifstream& ifs, UINT count, std::vector<GeometryGenerator::Vertex>& vertices, bool skinned = false);
	void LoadTriangles(std::ifstream& ifs, UINT count, std::vector<UINT>& indices);
	void LoadSubsets(std::ifstream& ifs, UINT count, std::vector<Subset>& subsets);
	void LoadMaterials(std::ifstream& ifs, UINT count, std::vector<Model3DMaterial>& materials);
	
	void LoadBoneOffsets(std::ifstream& ifs, UINT count, std::vector<XMFLOAT4X4>& offsets);
	void LoadBoneHierarchy(std::ifstream& ifs, UINT count, std::vector<UINT>& hierarchy);
	void LoadAnimationClips(std::ifstream& ifs, UINT BoneCount, UINT ClipCount, std::map<std::string, AnimationClip>& animations);
	void LoadAnimation(std::ifstream& ifs, UINT count, AnimationObject& animation);
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
	enum WindowCorner
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,
		FullWindow
	};

	DebugQuad();
	~DebugQuad();

	void Init(ID3D11Device* device, UINT WindowWidth, UINT WindowHeight, float WindowAspectRatio, WindowCorner position, float TextureAspectRatio);
	void OnResize(UINT WindowWidth, UINT WindowHeight, float WindowAspectRatio);
	void Draw(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv);
	void Draw(ID3D11DeviceContext* context, std::vector<ID3D11ShaderResourceView*> SRVs);

private:
	WindowCorner mPosition;
	float mQuadAspectRatio;

	UINT mWindowWidth;
	UINT mWindowHeight;

	ID3D11Buffer* mDebugQuadCB;

	struct DebugQuadCB
	{
		XMFLOAT4X4 WorldViewProj;
		XMFLOAT2 DebugQuadSize;
		XMFLOAT2 padding;
	};

	static_assert((sizeof(DebugQuadCB) % 16) == 0, "constant buffer size must be 16-byte aligned");
};

class ShadowMap
{
	UINT mWidth;
	UINT mHeight;

	ID3D11DepthStencilView* mDSV;
	ID3D11ShaderResourceView* mSRV;

	D3D11_VIEWPORT mViewport;

	ID3D11Buffer* mPerObjectCB;

	ID3D11VertexShader* mVertexShader[2];
	ID3D11InputLayout* mInputLayout[2];
	ID3D11PixelShader* mPixelShader[2];

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

	static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11VertexShader* GetVS(bool IsSkinned = false);
	ID3D11InputLayout* GetIL(bool IsSkinned = false);
	ID3D11PixelShader* GetPS(bool IsAlphaClipping = false);
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
	ID3D11VertexShader* mNormalDepthVS[2];
	ID3D11InputLayout* mNormalDepthIL[2];
	ID3D11PixelShader* mNormalDepthPS[2];
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
		XMFLOAT4X4 WorldInverseTranspose;
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
	ID3D11VertexShader* GetNormalDepthVS(bool IsSkinned = false);
	ID3D11InputLayout* GetNormalDepthIL(bool IsSkinned = false);
	ID3D11PixelShader* GetNormalDepthPS(bool IsAlphaClipping = false);

	ID3D11ShaderResourceView*& GetNormalDepthSRV();
	ID3D11ShaderResourceView*& GetAmbientMapSRV();
	ID3D11RenderTargetView*& GetAmbientMapRTV();

	DebugQuad mDebugQuad;
};

class TerrainObject
{
public:
	struct InitInfo
	{
		std::wstring HeightMapFileName;
		std::vector<std::wstring> LayerMapFileName;
		std::wstring BlendMapFileName;
		float HeightScale;
		UINT HeightMapWidth;
		UINT HeightMapDepth;
		float CellSpacing;
	};

	ID3D11VertexShader* mVertexShader;
	ID3D11InputLayout* mInputLayout;
	ID3D11HullShader* mHullShader;
	ID3D11DomainShader* mDomainShader;
	ID3D11PixelShader* mPixelShader;

	ID3D11Buffer* mPatchQuadVB;
	ID3D11Buffer* mPatchQuadIB;

	struct VertexData
	{
		XMFLOAT3 position;
		XMFLOAT2 TexCoord;
		XMFLOAT2 BoundsY;
	};

	InitInfo mInitInfo;

	XMMATRIX mWorld;
	Material mMaterial;

	ID3D11ShaderResourceView* mHeightMapSRV;
	ID3D11ShaderResourceView* mLayerMapArraySRV;
	ID3D11ShaderResourceView* mBlendMapSRV;

	ID3D11SamplerState* mHeightMapSS;

	UINT mPatchQuadVertices;
	UINT mPatchQuadFaces;

private:
	void LoadHeightMap(TextureManager& manager);
	void SmoothHeightMap();
	bool IsInBounds(int i, int j);
	float average(int i, int j);
	void ComputePatchBoundsY(UINT i, UINT j);

	// each patch has CellsPerPatch cells and CellsPerPatch+1 vertices
	// 64 = max tessellation factor
	static const int CellsPerPatch = 64;

	UINT mPatchQuadRows;
	UINT mPatchQuadCols;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<float> mHeightMap;

public:
	TerrainObject();
	~TerrainObject();

	float GetWidth() const;
	float GetDepth() const;
	float GetHeight(float x, float z) const;

	void init(ID3D11Device* device, ID3D11DeviceContext* context, TextureManager& manager, const InitInfo& info);
};

class ParticleSystem
{
	UINT mMaxParticles;
	bool mFirstRun;

	float mGameTime;
	float mTimeStep;
	float mAge;

	XMFLOAT3 mEmitPosW;
	XMFLOAT3 mEmitDirW;

	ID3D11VertexShader* mVertexShader[2];
	ID3D11InputLayout* mInputLayout[2];
	ID3D11GeometryShader* mGeometryShader[2];
	ID3D11PixelShader* mPixelShader;
	
	ID3D11Buffer* mConstantBuffer;

	ID3D11DepthStencilState* mDisableDepthDSS;
	ID3D11DepthStencilState* mNoDepthWritesDSS;

	ID3D11BlendState* mAdditiveBlending;

	struct ConstantBuffer
	{
		XMFLOAT4 EyePosW;

		XMFLOAT4 EmitPosW;
		XMFLOAT4 EmitDirW;

		float GameTime;
		float TimeStep;
		XMFLOAT2 padding;

		XMFLOAT4X4 ViewProj;
	};;

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mInitVB;
	ID3D11Buffer* mStreamOutVB;
	ID3D11Buffer* mDrawVB;

	ID3D11ShaderResourceView* mTextureArraySRV;
	ID3D11ShaderResourceView* mRandomTextureSRV;

public:
	ParticleSystem();
	~ParticleSystem();

	struct VertexData
	{
		XMFLOAT3 InitPosition;
		XMFLOAT3 InitVelocity;
		XMFLOAT2 size;
		float age;
		UINT type;
	};

	void init(ID3D11Device* device,
			  TextureManager& manager,
			  const std::wstring& EffectName,
			  const std::vector<std::wstring>& TextureArrayName,
			  UINT RandomTextureIndex,
			  const XMFLOAT3& EmitPos,
			  UINT MaxParticle);

	void reset();
	void update(float dt, float GameTime);
	void draw(ID3D11DeviceContext* context, const CameraObject& camera);

	XMFLOAT3& GetEmitPos() { return mEmitPosW; }
};

#endif // D3DAPP_H