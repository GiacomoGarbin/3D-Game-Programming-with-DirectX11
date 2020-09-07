#include <D3DApp/D3DApp.h>

#include <cassert>
#include <string>
#include <array>
#include <sstream>

#include <d3dcompiler.h>

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
	ID3D11Buffer* mVertexBuffer;
	ID3D11Buffer* mIndexBuffer;

	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProj;

	//struct ConstantBuffer
	//{
	//	XMFLOAT4X4 mWorldViewProj;
	//};

	struct cbPerObject
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		GameObject::Material mMaterial;
	};

	struct cbPerFrame
	{
		LightDirectional mLightDirectional;
		LightPoint mLightPoint;
		LightSpot mLightSpot;

		XMFLOAT3 mEyePositionW;
		float pad;
	};

	//ID3D11Buffer* mConstantBuffer;

	ID3D11Buffer* mCbPerObject;
	ID3D11Buffer* mCbPerFrame;

	ID3D11RasterizerState* mRasterizerState;

	ID3D11VertexShader* mVertexShader;
	ID3D11PixelShader* mPixelShader;

	GeometryGenerator::Mesh mMesh;

	//GameObject mBox;
	//GameObject mGrid;
	//GameObject mSphere;
	//GameObject mCylinder;
	//GameObject mSkull;
	//std::array<GameObject, 10> mSpheres;
	//std::array<GameObject, 10> mCylinders;

	GameObject mGrid;
	GameObject mWaves;

	GeometryGenerator::Waves mWavesGenerator;

	std::vector<GameObject> mObjects;

	LightDirectional mDirLight;
	LightPoint mPointLight;
	LightSpot mSpotLight;
};

TestApp::TestApp()
	: D3DApp()
{
	mMainWindowTitle = "Hello D3D11 Cube";

	mWorld = XMMatrixIdentity();
	mView = XMMatrixIdentity();
	mProj = XMMatrixIdentity();

	mDirLight.mAmbient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLight.mDiffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLight.mSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLight.mDirection = XMFLOAT3(+0.57735f, -0.57735f, +0.57735f);

	mPointLight.mAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	mPointLight.mDiffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	mPointLight.mSpecular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	mPointLight.mAttenuation = XMFLOAT3(0.0f, 0.1f, 0.0f);
	mPointLight.mRange = 25;

	mSpotLight.mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mSpotLight.mDiffuse = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
	mSpotLight.mSpecular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mSpotLight.mAttenuation = XMFLOAT3(1.0f, 0.0f, 0.0f);
	mSpotLight.mCone = 96;
	mSpotLight.mRange = 10000;
}

TestApp::~TestApp()
{
	SafeRelease(mInputLayout);
	SafeRelease(mVertexBuffer);
	SafeRelease(mIndexBuffer);
	SafeRelease(mCbPerObject);
	SafeRelease(mCbPerFrame);
	SafeRelease(mRasterizerState);
	SafeRelease(mVertexShader);
	SafeRelease(mPixelShader);
}

bool TestApp::Init()
{
	if (D3DApp::Init())
	{
		ID3DBlob* pVertexShaderCode;

		// vertex shader
		{
			ID3DBlob* pCode;
			HR(D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mVertexShader));

			pVertexShaderCode = pCode;
		} // vertex shader

		// pixel shader
		{
			ID3DBlob* pCode;
			HR(D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mPixelShader));
		} // pixel shader

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &mInputLayout));
		} // input layout

		// mesh
		{
			//GeometryGenerator::Mesh box;
			//GeometryGenerator::Mesh grid;
			//GeometryGenerator::Mesh sphere;
			//GeometryGenerator::Mesh cylinder;
			//GeometryGenerator::Mesh skull;

			//GeometryGenerator::CreateBox(1, 1, 1, box);
			//GeometryGenerator::CreateGrid(20, 30, 60, 50, grid);
			//GeometryGenerator::CreateSphere(0.5f, 3, sphere);
			//GeometryGenerator::CreateCylinder(0.5f, 0.3f, 3, 20, 20, cylinder);
			//GeometryGenerator::CreateSkull(skull);

			//mBox.mVertexStart = 0;
			//mGrid.mVertexStart = mBox.mVertexStart + box.mVertices.size();
			//mSphere.mVertexStart = mGrid.mVertexStart + grid.mVertices.size();
			//mCylinder.mVertexStart = mSphere.mVertexStart + sphere.mVertices.size();
			//mSkull.mVertexStart = mCylinder.mVertexStart + cylinder.mVertices.size();

			//mBox.mIndexStart = 0;
			//mGrid.mIndexStart = mBox.mIndexStart + box.mIndices.size();
			//mSphere.mIndexStart = mGrid.mIndexStart + grid.mIndices.size();
			//mCylinder.mIndexStart = mSphere.mIndexStart + sphere.mIndices.size();
			//mSkull.mIndexStart = mCylinder.mIndexStart + cylinder.mIndices.size();

			//mBox.mIndexCount = box.mIndices.size();
			//mGrid.mIndexCount = grid.mIndices.size();
			//mSphere.mIndexCount = sphere.mIndices.size();
			//mCylinder.mIndexCount = cylinder.mIndices.size();
			//mSkull.mIndexCount = skull.mIndices.size();

			//mMesh.mVertices.insert(mMesh.mVertices.end(), box.mVertices.begin(), box.mVertices.end());
			//mMesh.mVertices.insert(mMesh.mVertices.end(), grid.mVertices.begin(), grid.mVertices.end());
			//mMesh.mVertices.insert(mMesh.mVertices.end(), sphere.mVertices.begin(), sphere.mVertices.end());
			//mMesh.mVertices.insert(mMesh.mVertices.end(), cylinder.mVertices.begin(), cylinder.mVertices.end());
			//mMesh.mVertices.insert(mMesh.mVertices.end(), skull.mVertices.begin(), skull.mVertices.end());

			//mMesh.mIndices.insert(mMesh.mIndices.end(), box.mIndices.begin(), box.mIndices.end());
			//mMesh.mIndices.insert(mMesh.mIndices.end(), grid.mIndices.begin(), grid.mIndices.end());
			//mMesh.mIndices.insert(mMesh.mIndices.end(), sphere.mIndices.begin(), sphere.mIndices.end());
			//mMesh.mIndices.insert(mMesh.mIndices.end(), cylinder.mIndices.begin(), cylinder.mIndices.end());
			//mMesh.mIndices.insert(mMesh.mIndices.end(), skull.mIndices.begin(), skull.mIndices.end());

			//mBox.mWorld = XMMatrixScaling(2, 1, 2) * XMMatrixTranslation(0, 0.5f, 0);
			//mGrid.mWorld = XMMatrixIdentity();
			//mCylinder.mWorld = XMMatrixIdentity();
			//mSkull.mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0, 1.5f, 0);

			//for (UINT i = 0; i < 5; i++)
			//{
			//	UINT e = i * 2 + 0;
			//	UINT o = i * 2 + 1;

			//	mSpheres.at(e).mVertexStart = mSphere.mVertexStart;
			//	mSpheres.at(o).mVertexStart = mSphere.mVertexStart;
			//	mSpheres.at(e).mIndexStart = mSphere.mIndexStart;
			//	mSpheres.at(o).mIndexStart = mSphere.mIndexStart;
			//	mSpheres.at(e).mIndexCount = mSphere.mIndexCount;
			//	mSpheres.at(o).mIndexCount = mSphere.mIndexCount;

			//	mSpheres.at(e).mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
			//	mSpheres.at(o).mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);

			//	mCylinders.at(e).mVertexStart = mCylinder.mVertexStart;
			//	mCylinders.at(o).mVertexStart = mCylinder.mVertexStart;
			//	mCylinders.at(e).mIndexStart = mCylinder.mIndexStart;
			//	mCylinders.at(o).mIndexStart = mCylinder.mIndexStart;
			//	mCylinders.at(e).mIndexCount = mCylinder.mIndexCount;
			//	mCylinders.at(o).mIndexCount = mCylinder.mIndexCount;

			//	mCylinders.at(e).mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
			//	mCylinders.at(o).mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
			//}

			//mObjects.push_back(mBox);
			//mObjects.push_back(mGrid);
			//for (GameObject& obj : mSpheres) mObjects.push_back(obj);
			//for (GameObject& obj : mCylinders) mObjects.push_back(obj);
			//mObjects.push_back(mSkull);

			GeometryGenerator::Mesh grid;
			GeometryGenerator::CreateGrid(160, 160, 50, 50, grid);

			auto GetHeight = [](float x, float z) -> float
			{
				return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
			};

			for (GeometryGenerator::Vertex& v : grid.mVertices)
			{
				v.mPosition.y = GetHeight(v.mPosition.x, v.mPosition.z);

				if (v.mPosition.y < -10)
				{
					v.mColor = XMFLOAT4(1.00f, 0.96f, 0.62f, 1);
				}
				else if (v.mPosition.y < 5)
				{
					v.mColor = XMFLOAT4(0.48f, 0.77f, 0.46f, 1);
				}
				else if (v.mPosition.y < 12)
				{
					v.mColor = XMFLOAT4(0.10f, 0.48f, 0.19f, 1);
				}
				else if (v.mPosition.y < 20)
				{
					v.mColor = XMFLOAT4(0.45f, 0.39f, 0.34f, 1);
				}
				else
				{
					v.mColor = XMFLOAT4(1.00f, 1.00f, 1.00f, 1);
				}
			}

			grid.ComputeNormals();

			mMesh.mVertices.insert(mMesh.mVertices.end(), grid.mVertices.begin(), grid.mVertices.end());
			mMesh.mIndices.insert(mMesh.mIndices.end(), grid.mIndices.begin(), grid.mIndices.end());

			mGrid.mVertexStart = 0;
			mGrid.mIndexStart = 0;
			mGrid.mIndexCount = grid.mIndices.size();
			mGrid.mWorld = XMMatrixIdentity();

			mGrid.mMaterial.mAmbient = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
			mGrid.mMaterial.mDiffuse = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
			mGrid.mMaterial.mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);

			mObjects.push_back(mGrid);

			GeometryGenerator::Mesh waves;
			GeometryGenerator::CreateGrid(160, 160, 200, 200, waves);

			mWavesGenerator.Init(200, 200, 0.8f, 0.03f, 3.25f, 0.4f);

			mMesh.mVertices.insert(mMesh.mVertices.end(), mWavesGenerator.mCurrVertices.begin(), mWavesGenerator.mCurrVertices.end());
			mMesh.mIndices.insert(mMesh.mIndices.end(), waves.mIndices.begin(), waves.mIndices.end());

			mWaves.mVertexStart = mGrid.mVertexStart + grid.mVertices.size();
			mWaves.mIndexStart = mGrid.mIndexStart + grid.mIndices.size();
			mWaves.mIndexCount = waves.mIndices.size();
			mWaves.mWorld = XMMatrixIdentity();

			mWaves.mMaterial.mAmbient = XMFLOAT4(0.137f, 0.42f, 0.556f, 1.0f);
			mWaves.mMaterial.mDiffuse = XMFLOAT4(0.137f, 0.42f, 0.556f, 1.0f);
			mWaves.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 96.0f);

			mObjects.push_back(mWaves);
		} // mesh

		// vertex buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mMesh.mVertices.size();
			desc.Usage = D3D11_USAGE_DYNAMIC; // D3D11_USAGE_IMMUTABLE
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mMesh.mVertices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mVertexBuffer));
		} // vertex buffer

		// index buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * mMesh.mIndices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = mMesh.mIndices.data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &data, &mIndexBuffer));
		} // index buffer

		// constant buffer per object
		{
			static_assert((sizeof(cbPerObject) % 16) == 0, "constant buffer size must be 16-byte aligned");

			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(cbPerObject);
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
			static_assert((sizeof(cbPerFrame) % 16) == 0, "constant buffer size must be 16-byte aligned");

			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(cbPerFrame);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			HR(mDevice->CreateBuffer(&desc, nullptr, &mCbPerFrame));

			mContext->PSSetConstantBuffers(1, 1, &mCbPerFrame);
		} // constant buffer per frame

		// rasterizer state
		{
			D3D11_RASTERIZER_DESC desc;
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_BACK;
			desc.FrontCounterClockwise = false;
			desc.DepthBias = 0;
			desc.DepthBiasClamp = 0;
			desc.SlopeScaledDepthBias = 0;
			desc.DepthClipEnable = true;
			desc.ScissorEnable = false;
			desc.MultisampleEnable = false;
			desc.AntialiasedLineEnable = false;

			HR(mDevice->CreateRasterizerState(&desc, &mRasterizerState));

			mContext->RSSetState(mRasterizerState);
		} // rasterizer state

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

	XMVECTOR EyePosition = XMVectorSet(x, y, z, 1);
	XMVECTOR FocusPosition = XMVectorSet(0, 0, 0, 1); // XMVectorZero();
	XMVECTOR UpDirection = XMVectorSet(0, 1, 0, 0);

	mView = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);

	static float BaseTime = 0;

	if ((mTimer.TotalTime() - BaseTime) >= 0.25f)
	{
		BaseTime += 0.25f;

		UINT i = 5 + rand() % 190;
		UINT j = 5 + rand() % 190;

		float r = GameMath::RandNorm(1.0f, 2.0f);

		mWavesGenerator.Disturb(i, j, r);
	}

	mWavesGenerator.Update(dt);

	//GeometryGenerator::Mesh::ComputeNormals(
	//	mMesh.mVertices.begin() + mWaves.mVertexStart, mMesh.mVertices.end(),
	//	mMesh.mIndices.begin() + mWaves.mIndexStart, mMesh.mIndices.end());

	D3D11_MAPPED_SUBRESOURCE MappedData;

	HR(mContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedData));

	GeometryGenerator::Vertex* vertex = reinterpret_cast<GeometryGenerator::Vertex*>(MappedData.pData);

	for (std::size_t i = 0; i < mMesh.mVertices.size(); i++)
	{
		if (i < mWaves.mVertexStart)
		{
			vertex[i] = mMesh.mVertices[i];
		}
		else
		{
			vertex[i] = mWavesGenerator[i - mWaves.mVertexStart];
		}
	}

	mContext->Unmap(mVertexBuffer, 0);

	auto GetHillHeight = [](float x, float z) -> float
	{
		return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
	};

	mPointLight.mPosition.x = 70 * std::cos(0.2f * mTimer.TotalTime());
	mPointLight.mPosition.z = 70 * std::sin(0.2f * mTimer.TotalTime());
	mPointLight.mPosition.y = std::max(GetHillHeight(mPointLight.mPosition.x, mPointLight.mPosition.z), -3.0f) + 10;

	XMStoreFloat3(&mSpotLight.mPosition, EyePosition);
	XMStoreFloat3(&mSpotLight.mDirection, XMVector3Normalize(FocusPosition - EyePosition));
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	XMFLOAT4 ClearColor = { 0.5f, 0.5f, 0.5f, 1.0f };

	mContext->ClearRenderTargetView(mRenderTargetView, &ClearColor.x);
	mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

	mContext->IASetInputLayout(mInputLayout);
	mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(GeometryGenerator::Vertex);
	UINT offset = 0;
	mContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
	mContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	mContext->RSSetState(mRasterizerState);

	mContext->VSSetShader(mVertexShader, nullptr, 0);
	mContext->PSSetShader(mPixelShader, nullptr, 0);

	// constant buffer per frame
	{
		cbPerFrame buffer;
		buffer.mLightDirectional = mDirLight;
		// XMStoreFloat4(&buffer.mLightDirectional.mAmbient, mDirLight.mAmbient);
		buffer.mLightPoint = mPointLight;
		buffer.mLightSpot = mSpotLight;
		buffer.mEyePositionW = mSpotLight.mPosition; // XMStoreFloat3(&buffer.mEyePositionW, mEyePosition);
		mContext->UpdateSubresource(mCbPerFrame, 0, 0, &buffer, 0, 0);
	}

	for (GameObject& obj : mObjects)
	{
		// constant buffer per object
		{
			cbPerObject buffer;
			XMStoreFloat4x4(&buffer.mWorld, obj.mWorld);
			XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj.mWorld));
			XMStoreFloat4x4(&buffer.mWorldViewProj, obj.mWorld * mView * mProj);
			buffer.mMaterial = obj.mMaterial;
			mContext->UpdateSubresource(mCbPerObject, 0, 0, &buffer, 0, 0);
		}

		mContext->DrawIndexed(obj.mIndexCount, obj.mIndexStart, obj.mVertexStart);
	}

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}