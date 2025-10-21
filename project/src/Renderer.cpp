//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	//m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	//delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	std::vector<Vector3> vertices_ndc
	{
		{   0.f,   0.5f, 1.f },
		{  0.5f,  -0.5f, 1.f },
		{ -0.5f,  -0.5f, 1.f },
	};

	std::vector<Vector3> vertices_screen
	{
		{ ((vertices_ndc[0].x + 1.f) / 2) * m_Width, ((1.f - vertices_ndc[0].y) / 2) * m_Height, 1.f },
		{ ((vertices_ndc[1].x + 1.f) / 2) * m_Width, ((1.f - vertices_ndc[1].y) / 2) * m_Height, 1.f },
		{ ((vertices_ndc[2].x + 1.f) / 2) * m_Width, ((1.f - vertices_ndc[2].y) / 2) * m_Height, 1.f },
	};

	//RENDER LOGIC
	for (int px{}; px < m_Width; ++px)
	{
		for (int py{}; py < m_Height; ++py)
		{
			ColorRGB finalColor{};

			if(PixelIsInTriangle(Vector2{float(px), float(py)}, vertices_screen))
			{
				finalColor = { 1.f, 1.f, 1.f };
			}
			else
			{
				finalColor = { 0.f, 0.f, 0.f };
			}

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	//Todo > W1 Projection Stage
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

bool Renderer::PixelIsInTriangle(const Vector2& pixel, const std::vector<Vector3>& triangle) const
{
	for(int idx{}; idx < 3; ++idx)
	{
		Vector2 v1{ triangle[idx].GetXY(), triangle[(idx + 1) % 3].GetXY() };
		Vector2 v2{ triangle[idx].GetXY(), pixel };
		
		float crossProduct = Vector2::Cross(v1, v2);

		if(crossProduct < 0)
		{
			return false;
		}
	}
	return true;
}