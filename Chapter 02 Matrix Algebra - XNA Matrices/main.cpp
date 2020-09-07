#include <iostream>
#include <directxmath.h>

using namespace DirectX;

std::ostream& operator<<(std::ostream& os, XMVECTOR const& v)
{
	XMFLOAT4 t;
	XMStoreFloat4(&t, v);

	os << '(' << t.x << ',' << t.y << ',' << t.z << ',' << t.w << ')' << '\n';

	return os;
}

std::ostream& operator<<(std::ostream& os, XMMATRIX const& m)
{
	XMFLOAT4X4 t;
	XMStoreFloat4x4(&t, m);

	for (std::size_t i = 0; i < 4; i++)
	{
		for (std::size_t j = 0; j < 4; j++)
		{
			os << t(i, j) << '\t';
		}
		os << '\n';
	}

	return os;
}

int main()
{
	XMMATRIX A
	(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 4.0f, 0.0f,
		1.0f, 2.0f, 3.0f, 1.0f
	);

	XMMATRIX B = XMMatrixIdentity();

	XMMATRIX C = A * B;

	XMMATRIX D = XMMatrixTranspose(A);

	XMVECTOR d = XMMatrixDeterminant(A);

	XMMATRIX E = XMMatrixInverse(&d, A);

	XMMATRIX F = A * E;

	std::cout << "A =\n";
	std::cout << A << '\n';

	std::cout << "B =\n";
	std::cout << B << '\n';

	std::cout << "C = A*B =\n";
	std::cout << C << '\n';

	std::cout << "D = transpose(A) =\n";
	std::cout << D << '\n';

	std::cout << "d = determinant(A) = " << d << '\n';
	
	std::cout << "E = inverse(A) =\n";
	std::cout << E << '\n';

	std::cout << "F = A*E =\n";
	std::cout << F << '\n';
	
	return 0;
}