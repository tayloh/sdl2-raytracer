
#include <iostream>
#include <glm/glm.hpp>

#define SDL_MAIN_HANDLED
#include "SDL.h"

#include "CornelBox.h"

// SDL2
SDL_Surface* screenHandle;
SDL_Window* windowHandle;

// RENDERER
const int SCREEN_WIDTH = 524;
const int SCREEN_HEIGHT = 524;
const int SAMPLES = 4;
const int MAX_RECURSION = 1;
const int RENDER_WIDTH = SCREEN_WIDTH * 2;
const int RENDER_HEIGHT = SCREEN_HEIGHT * 2;

Uint32* renderBuffer = new Uint32[RENDER_WIDTH * RENDER_HEIGHT]; // dangerous

float focalLength = SCREEN_HEIGHT;
glm::vec3 cameraPos(0.0f, 0.0f, -3.0f);
glm::mat3 R(1, 0, 0, 0, 1, 0, 0, 0, 1);
float cameraSpeed = 0.1f;

// SCENE
std::vector<Triangle> triangles;
glm::vec3 lightPos(0, -0.3, -0.5);
float intensityMultiplier = 1.0f;

//glm::vec3 lightColor = intensityMultiplier * glm::vec3(1, 1, 1);

glm::vec3 specularLightColor = intensityMultiplier * glm::vec3(0.8, 0.8, 0.8);
glm::vec3 diffuseLightColor = intensityMultiplier * glm::vec3(1, 1, 1);
glm::vec3 ambientLight = intensityMultiplier * glm::vec3(0.2, 0.2, 0.2);
float shininess = 2;

const float PI = 3.14159265358979323846;

float delta_theta = PI / 60;
float theta = 0;

struct Intersection
{
	glm::vec3 position;
	float distance;
	int triangleIndex;
};

// SDL2 Utility
void PutPixelSDL(SDL_Surface* surface, int x, int y, glm::vec3 color);
glm::vec3 GetPixelSDL(SDL_Surface* surface, int x, int y);
bool NoQuitMessageSDL();

// Rendering and Math
void Draw();
void updateCamera();
void updateRotation();
bool closestIntersection(glm::vec3 start, glm::vec3 dir, const std::vector<Triangle>& triangles, Intersection& closestIntersection);
glm::vec3 recursiveRaytrace(int recursionDepth, glm::vec3 color, glm::vec3 prevPos, Intersection& currentIntersection);
glm::vec3 directLight(const Intersection& i);

int main(int argc, char* args[])
{	

	// TODO: Cramers Rule, Anti-Aliasing, Recursive Raytracing X, Multiple solids, Moving Camera X

	// Load the Cornel Box Model
	LoadTestModel(triangles);

	// Initialize SDL
	SDL_SetMainReady();

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		std::cout << "Could not initialize SDL: " << SDL_GetError() << std::endl;
		return 1;
	}
	else
	{
		windowHandle = SDL_CreateWindow(
			"SDL2 Raytracer",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_SWSURFACE
		);

		if (windowHandle == NULL)
		{
			std::cout << "SDL Window could not be created: " << SDL_GetError() << std::endl;
			return 1;
		}
		else
		{
			screenHandle = SDL_GetWindowSurface(windowHandle);
		}
	}

	int t = SDL_GetTicks();

	// Render Loop
	while (NoQuitMessageSDL())
	{
		Draw();
		updateCamera();

		int t2 = SDL_GetTicks();
		float dt = float(t2 - t);
		t = t2;
		std::cout << "Render time: " << dt << " ms." << std::endl;
	}

	//SDL_SaveBMP(screenHandle, "C:/Users/taylo/Desktop/src/cg/repos/sdl2-raytracer/images/image.bmp");

	delete[] renderBuffer; // Clear memory occupied by renderBuffer
	return 0;
}

void Draw()
{
	if (SDL_MUSTLOCK(screenHandle))
	{
		SDL_LockSurface(screenHandle);
	}


	// Ray trace
	for (int y = 0; y < SCREEN_HEIGHT; y++)
	{
		for (int x = 0; x < SCREEN_WIDTH; x++)
		{

			glm::vec3 dir(x - (SCREEN_WIDTH / 2), y - (SCREEN_HEIGHT / 2), focalLength);
			dir = R * dir;
			Intersection intersection;
			glm::vec3 color(0, 0, 0);

			if (closestIntersection(cameraPos, dir, triangles, intersection))
			{
				glm::vec3 lightIntensity = directLight(intersection);
			
				// Hack for setting reflective and non reflective surfaces
				if (triangles[intersection.triangleIndex].color == glm::vec3(0.45f, 0.45f, 0.45f)
					|| triangles[intersection.triangleIndex].color == glm::vec3(0.75f, 0.75f, 0.75f))
				{
					color = (lightIntensity + ambientLight) * triangles[intersection.triangleIndex].color;
				}
				else
				{
					color = (lightIntensity + ambientLight) * triangles[intersection.triangleIndex].color;
					color = recursiveRaytrace(0, color, cameraPos, intersection);
				}
			}

			PutPixelSDL(screenHandle, x, y, color);
		}
	}

	if (SDL_MUSTLOCK(screenHandle))
	{
		SDL_UnlockSurface(screenHandle);
	}

	SDL_UpdateWindowSurface(windowHandle);

}

glm::vec3 recursiveRaytrace(int recursionDepth, glm::vec3 color, glm::vec3 prevPos, Intersection& currentIntersection)
{
	if (recursionDepth == MAX_RECURSION) return color;
	else
	{
		recursionDepth++;
		glm::vec3 intersectPoint = currentIntersection.position;
		glm::vec3 d = glm::normalize(prevPos - intersectPoint);
		glm::vec3 n = glm::normalize(triangles[currentIntersection.triangleIndex].normal);
		glm::vec3 r = 2.0f * glm::max(glm::dot(d, n), 0.0f) * n - d;
		r = glm::normalize(r);

		if (closestIntersection(intersectPoint + 0.0001f * r, r, triangles, currentIntersection))
		{
			glm::vec3 lightIntensity = directLight(currentIntersection);
			color = color * (lightIntensity + ambientLight) * triangles[currentIntersection.triangleIndex].color;
			return recursiveRaytrace(recursionDepth, color, intersectPoint, currentIntersection);
		}
		else
		{
			return color;
		}
	}
}

bool closestIntersection(glm::vec3 start, glm::vec3 dir, const std::vector<Triangle>& triangles, Intersection& closestIntersection)
{
	float minimumDistance = std::numeric_limits<float>::max();
	bool found = false;

	for (int i = 0; i < triangles.size(); i++)
	{
		glm::vec3 v0 = triangles[i].v0;
		glm::vec3 v1 = triangles[i].v1;
		glm::vec3 v2 = triangles[i].v2;
		glm::vec3 e1 = v1 - v0;
		glm::vec3 e2 = v2 - v0;
		glm::vec3 B = start - v0;
		glm::mat3 A(-dir, e1, e2);
		glm::vec3 X = glm::inverse(A) * B;

		float t = X.x;
		float u = X.y;
		float v = X.z;

		if (0 <= u && 0 <= v && (u + v) <= 1 && 0 <= t)
		{
			glm::vec3 pos = v0 + (u * e1) + (v * e2);
			float dis = glm::distance(start, pos);

			if (dis <= minimumDistance)
			{
				minimumDistance = dis;
				closestIntersection.triangleIndex = i;
				closestIntersection.position = pos;
				closestIntersection.distance = dis;
				found = true;
			}
		}
	}
	return found;
}

glm::vec3 directLight(const Intersection& i)
{
	// glm::vec3 directLight = lightColor;
	glm::vec3 specularLight = specularLightColor;
	glm::vec3 diffuseLight = diffuseLightColor;
	glm::vec3 n = glm::normalize(triangles[i.triangleIndex].normal);
	glm::vec3 r = glm::normalize(lightPos - i.position);
	float distanceToLight = glm::distance(i.position, lightPos);

	Intersection shadowIntersection;
	
	// Add small distance to prevent self intersection
	if (closestIntersection(i.position + r * 0.000001f, r, triangles, shadowIntersection))
	{ 
		// Make sure it didn't intersect itself
		if (shadowIntersection.triangleIndex != i.triangleIndex &&
			shadowIntersection.distance < distanceToLight)
		{
			//directLight = { 0, 0, 0 };
			specularLight = { 0, 0, 0 };
			diffuseLight = { 0, 0, 0 };
		}
	}

	// Old lighting model
	// Area of sphere (assume light spreads in a sphere) = 4*pi*r^2

	//float area = 4 * PI * (distanceToLight * distanceToLight);
	//glm::vec3 powerPerArea = directLight / area;
	//directLight = powerPerArea * glm::max(glm::dot(r, n), 0.0f);


	// Phong Illumination Model (no ambient)
	// Specular
	glm::vec3 reflectionVector = 2.0f * glm::max(glm::dot(r, n), 0.0f) * n - r;
	reflectionVector = glm::normalize(reflectionVector);

	//reflectionVector = (glm::normalize(cameraPos - i.position) + n) / 2.0f; half angle

	float rdotv = glm::max(glm::dot(reflectionVector, glm::normalize(cameraPos - i.position)), 0.0f);

	specularLight = glm::pow(rdotv, shininess) * specularLight / (distanceToLight * distanceToLight);

	// Diffuse
	diffuseLight = glm::max(glm::dot(n, r), 0.0f) * diffuseLight / (distanceToLight * distanceToLight);

	glm::vec3 directLight = specularLight + diffuseLight;

	return directLight;
}


void PutPixelSDL(SDL_Surface* surface, int x, int y, glm::vec3 color)
{
	if (x < 0 || surface->w <= x || y < 0 || surface->h <= y)
		return;

	Uint8 r = Uint8(glm::clamp(255 * color.r, 0.f, 255.f));
	Uint8 g = Uint8(glm::clamp(255 * color.g, 0.f, 255.f));
	Uint8 b = Uint8(glm::clamp(255 * color.b, 0.f, 255.f));

	Uint32* p = (Uint32*)surface->pixels + y * surface->pitch / 4 + x;
	*p = SDL_MapRGB(surface->format, r, g, b);
}

glm::vec3 GetPixelSDL(SDL_Surface* surface, int x, int y)
{
	if (x < 0 || surface->w <= x || y < 0 || surface->h <= y)
		return glm::vec3(0, 0, 0);

	Uint32* p = (Uint32*)surface->pixels + y * surface->pitch / 4 + x;

	Uint8 r;
	Uint8 g;
	Uint8 b;

	SDL_GetRGB(*p, surface->format, &r, &g, &b);

	glm::vec3 pixelColor(r, g, b);
	
	pixelColor.x = glm::clamp(pixelColor.x / 255, 0.0f, 1.0f);
	pixelColor.y = glm::clamp(pixelColor.y / 255, 0.0f, 1.0f);
	pixelColor.z = glm::clamp(pixelColor.z / 255, 0.0f, 1.0f);

	return pixelColor;
}

bool NoQuitMessageSDL()
{
	SDL_Event e;
	while (SDL_PollEvent(&e)) 
	{
		if (e.type == SDL_QUIT) return false;
		if (e.type == SDL_KEYDOWN)
		{
			// Separate if statement to make it clear it's a key event
			if (e.key.keysym.sym == SDLK_ESCAPE) return false;
		}
	}
	return true;
}

void updateCamera()
{
	glm::vec3 forward(R[2][0], R[2][1], R[2][2]);
	glm::vec3 right(R[0][0], R[0][1], R[0][2]);
	glm::vec3 down(R[1][0], R[1][1], R[1][2]);

	const Uint8* keystate = SDL_GetKeyboardState(0);

	if (keystate[SDL_SCANCODE_UP])
	{
		cameraPos += forward * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_DOWN])
	{
		cameraPos -= forward * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_LEFT])
	{
		cameraPos -= right * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_RIGHT])
	{
		cameraPos += right * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_R])
	{
		cameraPos = { 0, 0, -3 };
		lightPos = { 0, -0.3, -0.5 };
		R = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	}
	if (keystate[SDL_SCANCODE_Y])
	{
		theta -= delta_theta;
		updateRotation();
	}
	if (keystate[SDL_SCANCODE_T])
	{
		theta += delta_theta;
		updateRotation();
	}
	if (keystate[SDL_SCANCODE_W])
	{
		lightPos += forward * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_S])
	{
		lightPos -= forward * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_Q])
	{
		lightPos += down * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_E])
	{
		lightPos -= down * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_A])
	{
		lightPos -= right * cameraSpeed;
	}
	if (keystate[SDL_SCANCODE_D])
	{
		lightPos += right * cameraSpeed;
	}
}
void updateRotation()
{
	R[0][0] = glm::cos(theta);
	R[2][2] = glm::cos(theta);
	R[2][0] = -glm::sin(theta);
	R[0][2] = glm::sin(theta);
}