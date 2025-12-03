//External includes
#include "SDL.h"
#include "SDL_surface.h"
#include <iostream>

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"
#include "BRDFs.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow),
	m_RenderMode{ RenderMode::COMBINED },
	m_LightDirection{ .577f, -.577f, .577f }, //Directional Light
	m_LightIntensity{ 7.f },
	m_RotateTimer{},
	m_Shininess{ 25.f },
	m_Ambient{ 0.03f, 0.03f, 0.03f }
{
	//Initialize
	m_Meshes.reserve(10);
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_AllPixels = m_Width * m_Height;
	m_PixelIndices.reserve(m_AllPixels);
	for (int idx{}; idx < m_AllPixels; ++idx) m_PixelIndices.emplace_back(idx);

	//Initialize Camera
	m_Camera.origin = Vector3{ 0.0f, 5.0f, -64.f };
	m_Camera.fov = tanf((45.0f * TO_RADIANS) / 2.0f);
	m_Camera.near = .1f;
	m_Camera.far = 100.0f;
	m_Camera.CalculateProjectionMatrix();
	m_Camera.CalculateViewMatrix();

	//Load Textures
	m_pDiffuseMap = std::unique_ptr<Texture>(Texture::LoadFromFile("Resources/vehicle_diffuse.png"));

	m_pGlossMap = std::unique_ptr<Texture>(Texture::LoadFromFile("Resources/vehicle_gloss.png"));
	m_pNormalMap = std::unique_ptr<Texture>(Texture::LoadFromFile("Resources/vehicle_normal.png"));
	m_pSpecularMap = std::unique_ptr<Texture>(Texture::LoadFromFile("Resources/vehicle_specular.png"));

	//Initialize Depth Buffer
	m_pDepthBufferPixels = new float[m_AllPixels];
	for (int idx{}; idx < m_AllPixels; ++idx) m_pDepthBufferPixels[idx] = FLT_MAX;

	//Load Mesh
	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};
	Utils::ParseOBJ("Resources/vehicle.obj", vertices, indices);
	m_Meshes.emplace_back(Mesh{ vertices, indices, PrimitiveTopology::TriangleList });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	m_pDepthBufferPixels = nullptr;

	if (m_pBackBuffer) {
		SDL_FreeSurface(m_pBackBuffer);
		m_pBackBuffer = nullptr;
	}

	if (m_pFrontBuffer) {
		SDL_FreeSurface(m_pFrontBuffer);
		m_pFrontBuffer = nullptr;
	}

	if (m_pWindow) {
		SDL_DestroyWindow(m_pWindow);
		m_pWindow = nullptr;
	}

	m_pBackBufferPixels = nullptr;
}

//Update function for the renderer
void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
	if (m_RotateMesh)
	{
		m_RotateTimer += pTimer->GetElapsed();
		m_Meshes[0].worldMatrix = Matrix::CreateRotationY(m_RotateTimer);
	}
}

//Toggle between render modes
void Renderer::SwitchDepthBuffer()
{
	switch (m_RenderMode)
	{
	case RenderMode::DEPTHBUFFER:
		m_RenderMode = RenderMode::COMBINED;
		std::cout << "Current render mode : combined" << std::endl;
		break;
	default:
		m_RenderMode = RenderMode::DEPTHBUFFER;
		std::cout << "Current render mode : depth buffer" << std::endl;
		break;
	}
}

void Renderer::ToggleNormalMap()
{
	m_UseNormalMap = !m_UseNormalMap;

	std::cout << "Normal map is " << ((m_UseNormalMap) ? "On" : "Off") << std::endl;
}

void Renderer::ToggleRotateMesh()
{
	m_RotateMesh = !m_RotateMesh;

	std::cout << "Rotating mesh is " << ((m_RotateMesh) ? "On" : "Off") << std::endl;
}

//Cycle through RenderModes
void Renderer::SwitchRenderMode()
{
	int newMode = int(m_RenderMode) + 1;
	if (newMode > 3) newMode = 0;

	m_RenderMode = RenderMode(newMode);

	switch (m_RenderMode)
	{
	case RenderMode::COMBINED:
		std::cout << "Render mode: combined" << std::endl;
		break;
	case RenderMode::OBSERVEDAREA:
		std::cout << "Render mode: observed area" << std::endl;
		break;
	case RenderMode::DIFFUSE:
		std::cout << "Render mode: diffuse" << std::endl;
		break;
	case RenderMode::SPECULAR:
		std::cout << "Render mode: specular" << std::endl;
		break;
	}
}

void Renderer::Render()
{
	// Lock the back buffer for rendering
	SDL_LockSurface(m_pBackBuffer);
	SDL_FillRect(m_pBackBuffer, nullptr, ColorToUint32(colors::Gray));

	// Initialize depth buffer to maximum depth
	std::fill(m_pDepthBufferPixels, m_pDepthBufferPixels + (m_Width * m_Height), FLT_MAX);

	for (Mesh& mesh : m_Meshes)
	{
		int skipVertices = 0;
		int vertexIncrement = 0;
		size_t frustumVerticesCount = 0;

		// Determine parameters based on topology type
		if (mesh.primitiveTopology == PrimitiveTopology::TriangleList)
		{
			skipVertices = 0;
			vertexIncrement = 3;
			frustumVerticesCount = mesh.indices.size() / 3;
		}
		else if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
		{
			skipVertices = 2;
			vertexIncrement = 1;
			frustumVerticesCount = mesh.indices.size() - 2;
		}

		// Transform and project vertices
		VertexTransformationFunction(mesh.vertices, mesh.vertices_out, mesh.worldMatrix);

		// Adjust vertices to screen space
		for (auto& vertex : mesh.vertices_out)
		{
			if (vertex.inFrustum)
			{
				vertex.position.x = ((vertex.position.x + 1.0f) * 0.5f) * m_Width;
				vertex.position.y = ((1.0f - vertex.position.y) * 0.5f) * m_Height;
			}
		}

		// Process triangles
		for (size_t i = 0; i < mesh.indices.size() - skipVertices; i += vertexIncrement)
		{
			int idx0 = mesh.indices[i];
			int idx1 = mesh.indices[i + 1];
			int idx2 = mesh.indices[i + 2];

			if (!mesh.vertices_out[idx0].inFrustum ||
				!mesh.vertices_out[idx1].inFrustum ||
				!mesh.vertices_out[idx2].inFrustum)
			{
				continue;
			}

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
			{
				if (i % 2 != 0)
				{
					std::swap(idx1, idx2);
				}
			}

			Vector2 v0 = mesh.vertices_out[idx0].position.GetXY();
			Vector2 v1 = mesh.vertices_out[idx1].position.GetXY();
			Vector2 v2 = mesh.vertices_out[idx2].position.GetXY();

			int minX = std::clamp((int)(std::min({ v0.x, v1.x, v2.x }) - 1.0f), 0, m_Width);
			int minY = std::clamp((int)(std::min({ v0.y, v1.y, v2.y }) - 1.0f), 0, m_Height);
			int maxX = std::clamp((int)(std::max({ v0.x, v1.x, v2.x }) + 1.0f), 0, m_Width);
			int maxY = std::clamp((int)(std::max({ v0.y, v1.y, v2.y }) + 1.0f), 0, m_Height);

			Vector2 edges[] = { v1 - v0, v2 - v1, v0 - v2 };

			// Rasterize triangle
			for (int px = minX; px < maxX; ++px)
			{
				for (int py = minY; py < maxY; ++py)
				{
					Vector2 pixelPos{ (float)px + 0.5f, (float)py + 0.5f };

					float edgeTests[] = {
						Vector2::Cross(edges[0], pixelPos - v0),
						Vector2::Cross(edges[1], pixelPos - v1),
						Vector2::Cross(edges[2], pixelPos - v2)
					};

					if ((edgeTests[0] > 0 && edgeTests[1] > 0 && edgeTests[2] > 0) ||
						(edgeTests[0] < 0 && edgeTests[1] < 0 && edgeTests[2] < 0))
					{
						float totalWeight = edgeTests[0] + edgeTests[1] + edgeTests[2];
						float w0 = edgeTests[1] / totalWeight;
						float w1 = edgeTests[2] / totalWeight;
						float w2 = edgeTests[0] / totalWeight;

						float interpolatedDepth = 1.0f / (
							(1.0f / mesh.vertices_out[idx0].position.z) * w0 +
							(1.0f / mesh.vertices_out[idx1].position.z) * w1 +
							(1.0f / mesh.vertices_out[idx2].position.z) * w2);

						int bufferIdx = px + (py * m_Width);

						if (interpolatedDepth < m_pDepthBufferPixels[bufferIdx])
						{
							// Interpolate attributes for shading
							Vector2 interpolatedUV = (
								mesh.vertices[idx0].uv * w0 +
								mesh.vertices[idx1].uv * w1 +
								mesh.vertices[idx2].uv * w2) * interpolatedDepth;

							Vector3 interpolatedNormal = (
								mesh.vertices_out[idx0].normal * w0 +
								mesh.vertices_out[idx1].normal * w1 +
								mesh.vertices_out[idx2].normal * w2) * interpolatedDepth;

							Vector3 interpolatedTangent = (
								mesh.vertices_out[idx0].tangent * w0 +
								mesh.vertices_out[idx1].tangent * w1 +
								mesh.vertices_out[idx2].tangent * w2) * interpolatedDepth;

							Vector3 interpolatedViewDirection = (
								mesh.vertices_out[idx0].viewDirection * w0 +
								mesh.vertices_out[idx1].viewDirection * w1 +
								mesh.vertices_out[idx2].viewDirection * w2) * interpolatedDepth;

							interpolatedNormal.Normalize();
							interpolatedTangent.Normalize();
							interpolatedViewDirection.Normalize();

							Vertex_Shader shaderData{
								interpolatedDepth,
								interpolatedUV,
								interpolatedNormal,
								interpolatedTangent,
								interpolatedViewDirection };

							PixelShading(shaderData, bufferIdx);
							m_pDepthBufferPixels[bufferIdx] = interpolatedDepth;
						}
					}
				}
			}
		}
	}

	// Update the SDL surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, nullptr, m_pFrontBuffer, nullptr);
	SDL_UpdateWindowSurface(m_pWindow);
}

Uint32 Renderer::ColorToUint32(const ColorRGB& color)
{
	return (Uint8(1.0f * 255) << 24) +	
		(Uint8(color.r * 255) << 16) +	
		(Uint8(color.g * 255) << 8) +
		Uint8(color.b * 255);			
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices, std::vector<Vertex_Out>& verticesOut, const Matrix& worldMatrix) const
{
	const Matrix worldViewProjectionMatrix{ worldMatrix * m_Camera.viewMatrix * m_Camera.projMatrix };

	int idx{ 0 };

	for (Vertex_Out& vertexOut : verticesOut)
	{
		vertexOut.position = worldViewProjectionMatrix.TransformPoint(vertices[idx].position.ToPoint4());

		vertexOut.position.x /= vertexOut.position.w;
		vertexOut.position.y /= vertexOut.position.w;
		vertexOut.position.z /= vertexOut.position.w;
		vertexOut.position.w = 1.0f / vertexOut.position.w;

		vertexOut.normal = worldMatrix.TransformVector(vertices[idx].normal).Normalized();
		vertexOut.tangent = worldMatrix.TransformVector(vertices[idx].tangent).Normalized();
		vertexOut.viewDirection = (worldMatrix.TransformPoint(vertices[idx].position) - m_Camera.origin).Normalized();
		vertexOut.inFrustum = InsideFrustum(vertexOut);

		++idx;
	}
}

bool Renderer::InsideFrustum(const Vertex_Out& vertex) const
{
	return ((vertex.position.x >= -1.0f)
		&& (vertex.position.x <= 1.0f)
		&& (vertex.position.y >= -1.0f)
		&& (vertex.position.y <= 1.0f)
		&& (vertex.position.z >= 0.0f)
		&& (vertex.position.z <= 1.0f));
}


bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::PixelShading(const Vertex_Shader& shaderVertex, int indexBuffer)
{
	ColorRGB color{};
	Vector3 normal{};

	if (m_UseNormalMap)
	{
		const Vector3 biNormal{ Vector3::Cross(shaderVertex.normal, shaderVertex.tangent) };
		const Matrix tangentSpaceAxis{ shaderVertex.tangent, biNormal, shaderVertex.normal, Vector3::Zero };
		normal = tangentSpaceAxis.TransformVector(m_pNormalMap->SampleNormal(shaderVertex.uv));
	}
	else normal = shaderVertex.normal;

	const float observedArea{ std::clamp(Vector3::Dot(-m_LightDirection, normal), 0.0f, 1.0f) };

	switch (m_RenderMode)
	{
	case RenderMode::DEPTHBUFFER:
	{
		color = colors::White * Remap(shaderVertex.depth, 0.995f, 1.0f);
		break;
	}
	case RenderMode::OBSERVEDAREA:
	{
		color = ColorRGB{ observedArea, observedArea, observedArea };
		break;
	}
	case RenderMode::DIFFUSE:
	{
		color = observedArea * BRDF::Lambert(m_LightIntensity, m_pDiffuseMap->Sample(shaderVertex.uv));
		break;
	}
	case RenderMode::SPECULAR:
	{
		const float sampledSpecular{ m_pSpecularMap->Sample(shaderVertex.uv).r };
		const float sampledPhongExponent{ m_pGlossMap->Sample(shaderVertex.uv).r };
		color = observedArea * BRDF::Phong(sampledSpecular, sampledPhongExponent * m_Shininess, -m_LightDirection, shaderVertex.viewDirection, normal);
		break;
	}
	case RenderMode::COMBINED:
	{
		const float sampledSpecular{ m_pSpecularMap->Sample(shaderVertex.uv).r };
		const float sampledPhongExponent{ m_pGlossMap->Sample(shaderVertex.uv).r };
		color = m_pDiffuseMap->Sample(shaderVertex.uv) + BRDF::Phong(sampledSpecular, sampledPhongExponent * m_Shininess, -m_LightDirection, shaderVertex.viewDirection, normal) + m_Ambient;
		break;
	}
	}

	color.MaxToOne();

	m_pBackBufferPixels[indexBuffer] = SDL_MapRGB(m_pBackBuffer->format,
		static_cast<uint8_t>(color.r * 255),
		static_cast<uint8_t>(color.g * 255),
		static_cast<uint8_t>(color.b * 255));
}

float Renderer::Remap(float v, float min, float max) const
{
	float result{ (v - min) / (max - min) };
	return Clamp(result, 0.0f, 1.0f);
}




