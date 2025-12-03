#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Maths.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle)
			: origin(_origin),
			  fovAngle(_fovAngle),
			  fov(tanf((_fovAngle * TO_RADIANS) / 2.f))
		{
		}
		float fovAngle{ 90.f };
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		float totalPitch{};
		float totalYaw{};

		float aspectRatio{ 1.f };
		float near{ 1.f };
		float far{ 1000.f };

		Vector3 origin{};
		Vector3 forward{ Vector3::UnitZ };
		Vector3 up{ Vector3::UnitY };
		Vector3 right{ Vector3::UnitX };

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projMatrix{};


		void Initialize(float _fovAngle = 60.f, const Vector3& _origin = {0.f, 0.f, -10.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((_fovAngle * TO_RADIANS) / 2.f);
			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			viewMatrix = Matrix::CreateLookAtLH(origin, forward);
			// DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix()
		{
			projMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, near, far);
			// DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();
			const float moveSpeed = 20.0f * deltaTime;
			const float rotateSpeed = dae::TO_RADIANS * 5.0f * deltaTime;

			// Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

			if (pKeyboardState[SDL_SCANCODE_W] || pKeyboardState[SDL_SCANCODE_UP])
				origin += forward * moveSpeed;
			if (pKeyboardState[SDL_SCANCODE_S] || pKeyboardState[SDL_SCANCODE_DOWN])
				origin -= forward * moveSpeed;
			if (pKeyboardState[SDL_SCANCODE_D] || pKeyboardState[SDL_SCANCODE_RIGHT])
				origin += right * moveSpeed;
			if (pKeyboardState[SDL_SCANCODE_A] || pKeyboardState[SDL_SCANCODE_LEFT])
				origin -= right * moveSpeed;

			// Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			const bool isLeftMouseDown = (mouseState & SDL_BUTTON_LMASK) != 0;
			const bool isRightMouseDown = (mouseState & SDL_BUTTON_RMASK) != 0;

			if (isLeftMouseDown)
			{
				if (isRightMouseDown)
					origin += up * float(mouseY) * moveSpeed;
				else
				{
					origin -= forward * float(mouseY) * moveSpeed;
					totalYaw += float(mouseX) * rotateSpeed;
				}
			}
			else if (isRightMouseDown)
			{
				totalYaw += float(mouseX) * rotateSpeed;
				totalPitch -= float(mouseY) * rotateSpeed;
			}

			Matrix rotationMatrix = Matrix::CreateRotation(totalPitch, totalYaw, 0.0f);
			forward = rotationMatrix.TransformVector(Vector3::UnitZ).Normalized();

			// Update Matrices
			CalculateViewMatrix();
		}
	};
}
