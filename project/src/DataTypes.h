#pragma once
#include "Maths.h"
#include "vector"

namespace dae
{
	struct Vertex
	{
		Vector3 position{};
		ColorRGB color{colors::White};
		Vector2 uv{}; 
		Vector3 normal{}; 
		Vector3 tangent{}; 
		Vector3 viewDirection{}; 
	};

	struct Vertex_Out
	{
		bool inFrustum;
		Vector4 position{};
		Vector3 normal{};
		Vector3 tangent{};
		Vector3 viewDirection{};
	};

	struct Vertex_Shader
	{
		float depth;
		Vector2 uv{};
		Vector3 normal{};
		Vector3 tangent{};
		Vector3 viewDirection{};
	};

	enum class PrimitiveTopology
	{
		TriangleList,
		TriangleStrip
	};

	struct Mesh
	{
		Mesh(const std::vector<Vertex>& vertices_in, const std::vector<uint32_t>& indices_in, PrimitiveTopology primitiveTopology_in) :
			vertices{ vertices_in },
			indices{ indices_in },
			primitiveTopology{ primitiveTopology_in },
			vertices_out{},
			worldMatrix{}
		{
			vertices_out.reserve(vertices.size());

			for (int i{}; i < vertices.size(); ++i)
			{
				vertices_out.emplace_back(Vertex_Out{});
			}
		}


		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		PrimitiveTopology primitiveTopology;

		std::vector<Vertex_Out> vertices_out;
		Matrix worldMatrix;
	};
}
