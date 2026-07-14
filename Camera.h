#ifndef CAMERAHEADER
#define CAMERAHEADER
#include <fstream>
#include <future>
#include <iostream>

#include "../utils/MeshReader/ObjReader.cpp"
#include "../utils/Scene/sceneSchema.hpp"
#include "./Octree.h"

class Pixel {
   public:
	Pixel(Ponto position, ColorData color)
		: color(color), position(position) {}

	void setColor(ColorData color) { this->color = color; }
	void setPos(Ponto position) { this->position = position; }

	int getR() const { return min(1.0, color.r) * 255; }
	int getG() const { return min(1.0, color.g) * 255; }
	int getB() const { return min(1.0, color.b) * 255; }
	ColorData getColor() const { return {color.r, color.g, color.b}; }
	Ponto getPos() const { return position; }

   private:
	ColorData color;  // Store the color as a ColorData struct for easier
					  // manipulation
	Ponto position;	  // Position of the pixel in the image
};

class Camera {
	// Octree for spatial partitioning of objects in the scene
	Octree octree;
	CameraData data;
	LightData globalLight;
	vector<LightData> lightList;
	vector<vector<Pixel>> pixels;  // 3D vector to store RGB values and position of each pixel

   public:
	Camera(SceneData scene)
		: data(scene.camera),
		  globalLight(scene.globalLight),
		  lightList(scene.lightList) {
		// Initialize the pixel grid based on the image dimensions
		pixels = vector<vector<Pixel>>(
			data.image_height,
			vector<Pixel>(data.image_width, Pixel(Ponto(0, 0, 0), {0, 0, 0})));

		double step = 1.0 / std::max(data.image_width, data.image_height);

		Vetor forward = (data.lookat - data.lookfrom).normalized();
		Vetor right = forward.cross(data.upVector).normalized() *
					  step;	 // Scale right vector by the image width to get the
							 // correct pixel spacing
		Vetor up = right.cross(forward).normalized() *
				   step;  // Scale up vector by the image height to get the
						  // correct pixel spacing

		// Set the position of each pixel in the grid based on the camera's
		// lookfrom and lookat points
		Ponto centroid = data.lookfrom + forward * data.screen_distance;

		Ponto topLeftPoint = centroid + up * ((data.image_height - 1) / 2) -
							 right * ((data.image_width - 1) / 2);

		for (int i = 0; i < data.image_height; i++) {
			for (int j = 0; j < data.image_width; j++) {
				pixels[i][j].setPos(topLeftPoint - up * i +
									right * j);	 // Assuming the pixel positions
												 // are in the XY plane at Z=0
			}
		}
	}

	// cout << Vetor
	friend std::ostream& operator<<(std::ostream& os, const Camera& c) {
		return os << "LookFrom: " << c.data.lookfrom
				  << ", LookAt: " << c.data.lookat
				  << ", UpVector: " << c.data.upVector
				  << ", ImageWidth: " << c.data.image_width
				  << ", ImageHeight: " << c.data.image_height
				  << ", ScreenDistance: " << c.data.screen_distance;
	}

	// Getters
	Ponto getLookFrom() const { return data.lookfrom; }
	Ponto getLookAt() const { return data.lookat; }
	Vetor getUpVector() const { return data.upVector; }
	int getImageWidth() const { return data.image_width; }
	int getImageHeight() const { return data.image_height; }
	double getScreenDistance() const { return data.screen_distance; }

	void plotPixels(string filename) const {
		// Create a PPM file with C++ io file creation and write the pixel data
		// in the PPM format

		ofstream outputFile(filename + ".ppm", ios::trunc);

		outputFile << "P3" << endl;
		outputFile << data.image_width << " " << data.image_height << endl;
		outputFile << "255" << endl;

		for (const auto& row : pixels) {
			for (const auto& pixel : row) {
				outputFile << pixel.getR() << " " << pixel.getG() << " "
						   << pixel.getB() << std::endl;
			}
		}

		outputFile.close();
	}

	// Raycasting

	struct Coord {
		int x, y;
	};

	void render(vector<ObjectData>& objects) {
		const int tile_size = 32;
		vector<future<void>> futures;

		processObjects(objects);

		octree.insert(objects);
		octree.build();

		for (int i = 0; i < data.image_height; i += tile_size) {
			for (int j = 0; j < data.image_width; j += tile_size) {
				int ex = std::min(i + tile_size, data.image_height);
				int ey = std::min(j + tile_size, data.image_width);

				futures.push_back(
					async(
						launch::async,
						&Camera::rayTrace,
						this,
						std::ref(objects),
						Coord{i, j}, Coord{ex, ey}));
			}
		}

		for (auto& f : futures) {
			f.get();
		}

		// After all ray tracing tasks complete, project any debug 'line'
		// objects onto the pixel grid.
		drawLines(objects);
	}

	void renderAlt(vector<ObjectData>& objects) {
		const int tile_size = 32;

		processObjects(objects);

		octree.insert(objects);
		octree.build(4);

		for (int i = 0; i < data.image_height; i += tile_size) {
			for (int j = 0; j < data.image_width; j += tile_size) {
				int ex = std::min(i + tile_size, data.image_height);
				int ey = std::min(j + tile_size, data.image_width);

				this->rayTrace(objects, Coord{i, j}, Coord{ex, ey});
			}
		}

		// After all ray tracing tasks complete, project any debug 'line'
		// objects onto the pixel grid.
		drawLines(objects);
	}

	void rayTrace(vector<ObjectData>& objects, Coord start, Coord end) {
		// For each pixel, calculate the ray direction and check for
		// intersections with objects in the scene
		for (int i = start.x; i < end.x; i++) {
			for (int j = start.y; j < end.y; j++) {
				Pixel& pixel = pixels[i][j];
				Vetor rayDirection = (pixel.getPos() - data.lookfrom).normalized();

				pixel.setColor(raycast(data.lookfrom, rayDirection));
			}
		}
	}

	ColorData raycast(Ponto origin, Vetor direction, int depth = 0) {
		if (depth > 8) {	   // Limit the recursion depth to prevent infinite loops in case of reflective/refractive materials
			return {0, 0, 0};  // Return black color for rays that exceed the recursion depth
		}

		Intersection intersection = octree.raycast({origin, direction});

		ColorData pixelColor(0, 0, 0);

		if (!intersection.hit) {
			return pixelColor;	// Return black if no intersection
		}

		auto normal = intersection.normal;
		auto object = intersection.object;

		double dotProd = direction.dot(normal);
		Ponto point = origin + direction * intersection.distance;

		pixelColor = globalLight.color * object->material.ka;				  // Ambient contribution
		applyLightFactor(pixelColor, origin, direction, intersection);		  // Add contribution from each light source
		applyReflection(pixelColor, origin, direction, intersection, depth);  // Add reflection contribution if applicable
		applyRefraction(pixelColor, origin, direction, intersection, depth);  // Add refraction contribution if applicable

		return pixelColor;	// Return the color of the pixel based on the intersection and lighting calculations
	}

	void applyLightFactor(ColorData& pixelColor, Ponto& origin, Vetor& direction, Intersection& target) {
		auto normal = target.normal;
		auto object = target.object;
		auto point = origin + direction * target.distance;

		double dotProd = direction.dot(normal);
		bool comingFromInside = (dotProd > 0);

		for (auto& light : lightList) {
			Vetor toLight = light.pos - point;
			double distanceToLight = toLight.norm();

			ColorData lightFactor(1.0, 1.0, 1.0);  // 1.0 = fully lit, 0.0 = fully shadowed
			Ponto shadowOrigin = point + (normal * (comingFromInside ? -1e-4 : 1e-4));
			Vetor shadowDir = toLight.normalize();

			while (distanceToLight > 1e-4) {
				// Find the closest object blocking this specific segment of the shadow ray
				Intersection check = octree.raycast({shadowOrigin, shadowDir, distanceToLight});

				// If nothing blocks the ray up to the light, we are done
				if (!check.hit) break;

				auto shadowObj = check.object;

				// If we hit an opaque object (no transparency component)
				if (!shadowObj->material.kt.positive()) {
					lightFactor = ColorData(0, 0, 0);  // Complete shadow
					break;
				}

				// If we hit a transparent object, attenuate the light color
				lightFactor = lightFactor * shadowObj->material.kt;

				// If light factor drops to near zero, stop tracking to save performance
				if (lightFactor.r < 1e-3 && lightFactor.g < 1e-3 && lightFactor.b < 1e-3) {
					lightFactor = ColorData(0, 0, 0);
					break;
				}

				// Move the ray origin past the hit point to continue testing towards the light
				// Push slightly along the shadow ray direction to avoid re-hitting the same point
				shadowOrigin = shadowOrigin + shadowDir * check.distance + shadowDir * 1e-4;
				distanceToLight -= (check.distance + 1e-4);
			}

			// --- APPLY THE LIGHT FACTOR TO PHONG EQUATION ---
			if (lightFactor.positive()) {
				double diffuseIntensity = max(0.0, normal.dot(toLight));
				ColorData opacity = ColorData(1.0, 1.0, 1.0) - object->material.kt;
				ColorData colorFactor = light.color * lightFactor;

				// Multiply light.color by lightFactor to account for transparency tint
				pixelColor = pixelColor + (colorFactor * object->material.color * diffuseIntensity);
				pixelColor = pixelColor * opacity;	// Ensure we don't have negative colors due to floating point issues

				Vetor viewDir = (origin - point).normalized();
				Vetor reflectDir = (normal * 2.0 * toLight.dot(normal) - toLight).normalized();
				double specularIntensity = pow(max(0.0, viewDir.dot(reflectDir)), object->material.ns);

				pixelColor = pixelColor + (colorFactor * object->material.ks * specularIntensity);
			}
		}
	}

	void applyReflection(ColorData& pixelColor, Ponto& origin, Vetor& direction, Intersection& target, int depth) {
		auto normal = target.normal;
		auto object = target.object;
		auto point = origin + direction * target.distance;

		double dotProd = direction.dot(normal);

		// Handle reflections if the material has a reflective component
		if (object->material.kr.positive()) {
			Vetor reflectDir = (direction - normal * 2 * dotProd).normalized();
			Ponto origin = point + reflectDir * 1e-4;  // Offset to avoid self-intersection
			ColorData reflectColor = raycast(origin, reflectDir, depth + 1);
			pixelColor = pixelColor + reflectColor * object->material.kr;
		}
	}

	void applyRefraction(ColorData& pixelColor, Ponto& origin, Vetor& direction, Intersection& target, int depth) {
		auto normal = target.normal;
		auto object = target.object;
		auto point = origin + direction * target.distance;

		double dotProd = direction.dot(normal);

		// Handle refraction (Snell)
		if (object->material.kt.positive()) {
			// etai = incident IOR, etat = transmitted IOR
			double eta_in = 1.0;					 // Assumes air as the initial medium
			double eta_trans = object->material.ni;	 // IOR of the material
			Vetor N = normal.normalized();

			// Clamp dot and determine if we are entering or exiting
			double cosi = direction.normalized().dot(N);
			if (cosi < -1.0) cosi = -1.0;
			if (cosi > 1.0) cosi = 1.0;

			bool entering = (cosi < 0);
			if (entering) {	 // Ray is entering the surface: make cosi positive
				cosi = -cosi;
			} else {  // Ray is exiting the surface: swap IORs and flip normal
				double tmp = eta_in;
				eta_in = eta_trans;
				eta_trans = tmp;
				N = N * -1.0;
			}

			double eta = eta_in / eta_trans;
			double k = 1.0 - eta * eta * (1.0 - cosi * cosi);

			if (k < 0.0) {
				// Total internal reflection: treat as reflection
				Vetor reflDir = (direction - normal * 2.0 * dotProd).normalized();
				Ponto origin = point + normal * (entering ? 1e-4 : -1e-4);	// small offset along normal
				ColorData reflColor = raycast(origin, reflDir, depth + 1);
				pixelColor = pixelColor + reflColor * object->material.kt;
			} else {
				// Refracted direction (Snell's law)
				Vetor refractDir = (direction * eta + N * (eta * cosi - sqrt(k))).normalized();
				Ponto origin = point + normal * (entering ? -1e-4 : 1e-4);	// offset into transmitted medium
				ColorData refractColor = raycast(origin, refractDir, depth + 1);
				pixelColor = pixelColor + refractColor * object->material.kt;
			}
		}
	}

	void renderOld(vector<ObjectData>& objects) {
		const int tile_size = 32;
		vector<future<void>> futures;

		processObjects(objects);

		for (int i = 0; i < data.image_height; i += tile_size) {
			for (int j = 0; j < data.image_width; j += tile_size) {
				int ex = std::min(i + tile_size, data.image_height);
				int ey = std::min(j + tile_size, data.image_width);

				futures.push_back(
					async(
						launch::async,
						&Camera::rayTraceOld,
						this,
						std::ref(objects),
						Coord{i, j}, Coord{ex, ey}));
			}
		}

		for (auto& f : futures) {
			f.get();
		}

		// After all ray tracing tasks complete, project any debug 'line'
		// objects onto the pixel grid.
		drawLines(objects);
	}

	void rayTraceOld(vector<ObjectData>& objects, Coord start, Coord end) {
		// For each pixel, calculate the ray direction and check for
		// intersections with objects in the scene
		for (int i = start.x; i < end.x; i++) {
			for (int j = start.y; j < end.y; j++) {
				Pixel& pixel = pixels[i][j];
				Vetor rayDirection = (pixel.getPos() - data.lookfrom).normalized();

				pixel.setColor(rayCastOld(data.lookfrom, rayDirection, objects));
			}
		}
	}

	ColorData rayCastOld(Ponto rayOrigin, Vetor rayDirection, vector<ObjectData>& objects, int depth = 0, bool hitOnly = false) {
		if (depth > 8) {	   // Limit the recursion depth to prevent infinite loops in case of reflective/refractive materials
			return {0, 0, 0};  // Return black color for rays that exceed the recursion depth
		}

		// Check for intersections with objects in the scene
		// If an intersection is found, calculate the color based on the
		// material properties and lighting Set the pixel color accordingly

		double closestT = std::numeric_limits<double>::max();  // Placeholder for the closest intersection
		ObjectData* closestObject = nullptr;				   // Pointer to the closest intersected object
		Vetor intersectionNormal;							   // Normal at the intersection point, used for
															   // lighting calculations

		for (auto& obj : objects) {
			RayHit intersec = testIntersection(rayOrigin, rayDirection, obj, closestT);
			if (intersec.hit) {
				closestT = intersec.t;
				closestObject = &obj;
				intersectionNormal = intersec.normal;
			}
		}
		ColorData pixelColor(0, 0, 0);

		if (closestObject != nullptr) {
			double dotProd = rayDirection.dot(intersectionNormal);
			Ponto intersectionPoint = rayOrigin + rayDirection * closestT;

			pixelColor = globalLight.color * closestObject->material.ka;  // Ambient contribution

			for (auto& light : lightList) {
				bool comingFromInside = (dotProd > 0);
				Vetor toLight = (light.pos - intersectionPoint).normalized();

				ColorData lightFactor(1.0, 1.0, 1.0);  // 1.0 = fully lit, 0.0 = fully shadowed
				Ponto shadowRayOrigin = intersectionPoint + intersectionNormal * (comingFromInside ? -1e-4 : 1e-4);
				Vetor shadowRayDir = toLight;
				double distanceToLight = (light.pos - intersectionPoint).norm();

				bool isFirstSegment = true;
				while (distanceToLight > 1e-4) {
					double closestShadowT = distanceToLight;
					ObjectData* closestShadowObj = nullptr;
					RayHit currentShadowHit = {false, 0, Vetor(0, 0, 0)};

					// Find the closest object blocking this specific segment of the shadow ray
					for (auto& obj : objects) {
						// Prevent immediate self-intersection with the surface we just started from
						if (&obj == closestObject && isFirstSegment) {
							if (!comingFromInside) continue;
						}

						RayHit hit = testIntersection(shadowRayOrigin, shadowRayDir, obj, closestShadowT);
						if (hit.hit && hit.t < closestShadowT) {
							closestShadowT = hit.t;
							closestShadowObj = &obj;
							currentShadowHit = hit;
						}
					}

					// If nothing blocks the ray up to the light, we are done
					if (closestShadowObj == nullptr) {
						break;
					}

					// If we hit an opaque object (no transparency component)
					if (!closestShadowObj->material.kt.positive()) {
						lightFactor = ColorData(0, 0, 0);  // Complete shadow
						break;
					}

					// If we hit a transparent object, attenuate the light color
					lightFactor = lightFactor * closestShadowObj->material.kt;

					// If light factor drops to near zero, stop tracking to save performance
					if (lightFactor.r < 1e-3 && lightFactor.g < 1e-3 && lightFactor.b < 1e-3) {
						lightFactor = ColorData(0, 0, 0);
						break;
					}

					// Move the ray origin past the hit point to continue testing towards the light
					// Push slightly along the shadow ray direction to avoid re-hitting the same point
					shadowRayOrigin = shadowRayOrigin + shadowRayDir * closestShadowT + shadowRayDir * 1e-4;
					distanceToLight -= (closestShadowT + 1e-4);

					isFirstSegment = false;
				}

				// --- APPLY THE LIGHT FACTOR TO PHONG EQUATION ---
				if (lightFactor.positive()) {
					double diffuseIntensity = max(0.0, intersectionNormal.dot(toLight));
					ColorData opacity = ColorData(1.0, 1.0, 1.0) - closestObject->material.kt;
					ColorData colorFactor = light.color * lightFactor;

					// Multiply light.color by lightFactor to account for transparency tint
					pixelColor = pixelColor + (colorFactor * closestObject->material.color * diffuseIntensity);
					pixelColor = pixelColor * opacity;	// Ensure we don't have negative colors due to floating point issues

					Vetor viewDir = (rayOrigin - intersectionPoint).normalized();
					Vetor reflectDir = (intersectionNormal * 2.0 * toLight.dot(intersectionNormal) - toLight).normalized();
					double specularIntensity = pow(max(0.0, viewDir.dot(reflectDir)), closestObject->material.ns);

					pixelColor = pixelColor + (colorFactor * closestObject->material.ks * specularIntensity);
				}
			}

			// Handle reflections if the material has a reflective component
			if (closestObject->material.kr.positive()) {
				Vetor reflectDir = (rayDirection - intersectionNormal * 2 * dotProd).normalized();
				Ponto origin = intersectionPoint + reflectDir * 1e-4;  // Offset to avoid self-intersection
				ColorData reflectColor = rayCastOld(origin, reflectDir, objects, depth + 1, hitOnly);
				pixelColor = pixelColor + reflectColor * closestObject->material.kr;
			}

			// Handle refraction (Snell)
			if (closestObject->material.kt.positive()) {
				// etai = incident IOR, etat = transmitted IOR
				double eta_in = 1.0;							// Assumes air as the initial medium
				double eta_trans = closestObject->material.ni;	// IOR of the material
				Vetor N = intersectionNormal.normalized();

				// Clamp dot and determine if we are entering or exiting
				double cosi = rayDirection.normalized().dot(N);
				if (cosi < -1.0) cosi = -1.0;
				if (cosi > 1.0) cosi = 1.0;

				bool entering = (cosi < 0);
				if (entering) {	 // Ray is entering the surface: make cosi positive
					cosi = -cosi;
				} else {  // Ray is exiting the surface: swap IORs and flip normal
					double tmp = eta_in;
					eta_in = eta_trans;
					eta_trans = tmp;
					N = N * -1.0;
				}

				double eta = eta_in / eta_trans;
				double k = 1.0 - eta * eta * (1.0 - cosi * cosi);

				if (k < 0.0) {
					// Total internal reflection: treat as reflection
					Vetor reflDir = (rayDirection - intersectionNormal * 2.0 * dotProd).normalized();
					Ponto origin = intersectionPoint + intersectionNormal * (entering ? 1e-4 : -1e-4);	// small offset along normal
					ColorData reflColor = rayCastOld(origin, reflDir, objects, depth + 1, hitOnly);
					pixelColor = pixelColor + reflColor * closestObject->material.kt;
				} else {
					// Refracted direction (Snell's law)
					Vetor refractDir = (rayDirection * eta + N * (eta * cosi - sqrt(k))).normalized();
					Ponto origin = intersectionPoint + intersectionNormal * (entering ? -1e-4 : 1e-4);	// offset into transmitted medium
					ColorData refractColor = rayCastOld(origin, refractDir, objects, depth + 1, hitOnly);
					pixelColor = pixelColor + refractColor * closestObject->material.kt;
				}
			}
		}

		return pixelColor;
	}

	struct RayHit {
		bool hit;	   // Indicates if the ray hit an object or not
		double t;	   // Distance from ray origin to the hit point
		Vetor normal;  // The normal vector at the hit point, used for lighting
					   // calculations
	};

	RayHit testIntersection(
		const Ponto& rayOrigin, const Vetor& rayDirection, ObjectData& obj,
		const double maxT = std::numeric_limits<double>::max(),
		const bool hitOnly = false) {
		// Implement intersection logic based on the object type (e.g.,
		// sphere, plane, mesh) If an intersection is detected, calculate
		// the color contribution from the object's material and lighting
		RayHit hitResult{false, std::numeric_limits<double>::max(),
						 Vetor(0, 0, 0)};

		if (obj.objType == "sphere") {
			double radius = obj.numericData["radius"];
			Ponto center = obj.relativePos;

			//  (rayOrigin + rayDirection * t - center).dot(rayOrigin +
			//  rayDirection * t - center) = radius^2
			// Implement the quadratic formula to find the intersection points
			// (t values) and determine if the ray intersects the sphere through
			// delta (fromX-centerX + rayDirection.x * t)^2 + (fromY-centerY +
			// rayDirection.y * t)^2 + (fromZ-centerZ + rayDirection.z * t)^2 =
			// radius^2 rayDirection.dot(rayDirection) * t^2 + 2 * ((rayOrigin -
			// center).dot(rayDirection)) * t + (rayOrigin -
			// center).dot(rayOrigin - center) - radius^2 = 0

			Vetor oc = rayOrigin - center;
			double A = rayDirection.dot(rayDirection);
			double B = 2.0 * oc.dot(rayDirection);
			double C = oc.dot(oc) - radius * radius;
			double delta = B * B - 4.0 * A * C;

			// If delta < 0, no intersection; if delta = 0, one intersection
			if (delta >= 0) {
				double t, t1, t2;
				t1 = (-B - sqrt(delta)) / (2.0 * A);
				t2 = (-B + sqrt(delta)) / (2.0 * A);

				// We want the closest positive t value (in front of the camera)
				t = min(max(t1, 0.), max(t2, 0.));

				// Only consider hits in front of the camera
				if (t > 1e-4 && t < maxT) {
					hitResult = {
						.hit = true,
						.t = t,
						.normal = (rayOrigin + rayDirection * t - center)
									  .normalized()};
				}
			}
		} else if (obj.objType == "plane") {
			Ponto point_on_plane = obj.relativePos;
			Vetor normal = obj.vetorPointData["normal"].normalized();

			double dotNorm = rayDirection.dot(normal);

			// Ray is parallel to the plane, no intersection
			if (fabs(dotNorm) != 0) {
				double t = (point_on_plane - rayOrigin).dot(normal) / dotNorm;

				// Only consider hits in front of the camera
				if (t > 1e-4 && t < maxT) {
					hitResult = {.hit = true, .t = t, .normal = normal};
				}
			}

		} else if (obj.objType == "mesh") {
			vector<vector<Ponto>> faces = obj.facePoints;
			vector<Vetor> normals = obj.faceNormals;

			double closestT = maxT;

			// Iterate through each face and perform ray-triangle intersection
			for (size_t i = 0; i < faces.size(); ++i) {
				vector<Ponto>& face = faces[i];
				for (auto& point : face) {
					point =
						point +
						obj.relativePos;  // Apply the mesh's relative position
										  // to each vertex of the face
				}

				Vetor normal = normals[i];

				// Check for parallelism
				double dotNorm = rayDirection.dot(normal);
				if (fabs(dotNorm) == 0)
					continue;  // Ray is parallel to triangle plane

				// Compute intersection t with the triangle plane
				double t = (face[0] - rayOrigin).dot(normal) / dotNorm;

				// Must be in front of camera and closer than previous hit
				if (t > 0 && t < closestT) {
					// Intersection point
					Ponto P = rayOrigin + rayDirection * t;

					// Inside-triangle test using edge-cross tests
					Vetor C;
					C = (face[1] - face[0]).cross(P - face[0]);
					if (normal.dot(C) < 0) continue;
					C = (face[2] - face[1]).cross(P - face[1]);
					if (normal.dot(C) < 0) continue;
					C = (face[0] - face[2]).cross(P - face[2]);
					if (normal.dot(C) < 0) continue;

					// Passed all checks: it's inside the triangle
					closestT = t;
					hitResult = {.hit = true, .t = t, .normal = normal};
					if (hitOnly) break;
				}
			}
		}

		return hitResult;
	}

	// Project line objects over the pixel grid: for each pixel, if distance
	// from pixel center to the line segment <= threshold, color the pixel with
	// the line material color.
	void drawLines(const vector<ObjectData>& objects) {
		// First, collect all projected 2D lines to draw so multiple lines don't
		// interfere during projection computations.
		struct Line2D {
			int x0, y0, x1, y1;
			int radius;
			ColorData color;
		};
		std::vector<Line2D> lines;

		double step = 1.0 / std::max(data.image_width, data.image_height);

		Vetor forward = (data.lookat - data.lookfrom).normalized();
		Vetor right_unit = forward.cross(data.upVector).normalized();
		Vetor up_unit = right_unit.cross(forward).normalized();

		double sd = data.screen_distance;
		Vetor right_scaled = right_unit * step;
		Vetor up_scaled = up_unit * step;
		Ponto centroid = data.lookfrom + forward * sd;
		Ponto topLeft = centroid + up_scaled * ((data.image_height - 1) / 2.0) -
						right_scaled * ((data.image_width - 1) / 2.0);

		auto project_point = [&](const Ponto& pt, double& out_i,
								 double& out_j) -> bool {
			Vetor dir = (pt - data.lookfrom);
			double dirDotF = dir.normalized().dot(forward);
			if (dirDotF <= 1e-9) return false;	// behind camera or parallel
			double t = sd / dir.normalized().dot(forward);
			Ponto Pproj = data.lookfrom + dir.normalized() * t;

			Vetor d = Pproj - topLeft;
			double j = d.dot(right_unit) / step;
			double i = -d.dot(up_unit) / step;
			out_i = i;
			out_j = j;
			return true;
		};

		for (const auto& obj : objects) {
			if (obj.objType != "line") continue;

			Ponto p1 = obj.relativePos;
			Ponto p2 = p1;
			bool haveSecond = false;
			const std::vector<std::string> keys = {"vector", "direction", "to",
												   "end", "point2"};
			for (const auto& k : keys) {
				auto it = obj.vetorPointData.find(k);
				if (it != obj.vetorPointData.end()) {
					p2 = p1 + it->second;
					haveSecond = true;
					break;
				}
			}
			if (!haveSecond) {
				auto it = obj.vetorPointData.find("point2");
				if (it != obj.vetorPointData.end()) {
					p2 = Ponto(it->second.getX(), it->second.getY(),
							   it->second.getZ());
					haveSecond = true;
				}
			}
			if (!haveSecond) continue;

			double i0d, j0d, i1d, j1d;
			if (!project_point(p1, i0d, j0d)) continue;
			if (!project_point(p2, i1d, j1d)) continue;

			if (i0d == i1d && j0d == j1d)
				continue;  // Degenerate line that projects to a single point,
						   // skip

			int x0 = static_cast<int>(std::round(j0d));
			int y0 = static_cast<int>(std::round(i0d));
			int x1 = static_cast<int>(std::round(j1d));
			int y1 = static_cast<int>(std::round(i1d));

			double threshold = max(0.02, sd * 0.02);
			auto it_size = obj.numericData.find("size");
			if (it_size != obj.numericData.end() && it_size->second > 0.0)
				threshold = it_size->second;
			double pixel_unit = up_scaled.norm();
			int radius = std::max(
				1,
				static_cast<int>(std::ceil(threshold / (pixel_unit + 1e-12))));

			// Prefer object-level root "color" (stored in
			// vetorPointData["color"]) if present
			ColorData curColor;
			auto itc = obj.vetorPointData.find("color");
			if (itc != obj.vetorPointData.end()) {
				curColor = ColorData(itc->second.getX(), itc->second.getY(),
									 itc->second.getZ());
			} else {
				curColor = obj.material.color;
			}

			lines.push_back(Line2D{x0, y0, x1, y1, radius, curColor});
		}

		// Rasterize all collected lines
		auto plot_disk = [&](int px, int py, const Line2D& L) {
			if (py < 0 || py >= data.image_height || px < 0 ||
				px >= data.image_width)
				return;
			for (int dy = -L.radius; dy <= L.radius; ++dy)
				for (int dx = -L.radius; dx <= L.radius; ++dx) {
					int nx = px + dx;
					int ny = py + dy;
					if (nx < 0 || nx >= data.image_width || ny < 0 ||
						ny >= data.image_height)
						continue;
					double dist = std::sqrt((double)dx * dx + (double)dy * dy);
					if (dist <= L.radius) pixels[ny][nx].setColor(L.color);
				}
		};

		for (const auto& L : lines) {
			int x0 = L.x0, y0 = L.y0, x1 = L.x1, y1 = L.y1;
			int dx = std::abs(x1 - x0);
			int sx = x0 < x1 ? 1 : -1;
			int dy = -std::abs(y1 - y0);
			int sy = y0 < y1 ? 1 : -1;
			int err = dx + dy;
			int cx = x0;
			int cy = y0;
			while (true) {
				plot_disk(cx, cy, L);
				if (cx == x1 && cy == y1) break;
				int e2 = 2 * err;
				if (e2 >= dy) {
					err += dy;
					cx += sx;
				}
				if (e2 <= dx) {
					err += dx;
					cy += sy;
				}
			}
		}
	}

   private:
	void processObjects(vector<ObjectData>& objects) {
		for (auto& obj : objects) {
			if (obj.objType == "mesh") {
				ObjReader mesh(obj.getProperty("path"));
				obj.facePoints = mesh.getFacePoints();
			}

			vector<pair<Ponto*, bool>>
				pointsToTransform;	// Pair of pointer to Ponto and a boolean
									// indicating if it's a normal vector
			vector<Vetor*> vectorsToTransform;
			vector<double*> scalarsToTransform;

			if (obj.objType == "sphere") {
				pointsToTransform.push_back({&obj.relativePos, false});
				scalarsToTransform.push_back(&obj.numericData["radius"]);
			} else if (obj.objType == "plane") {
				pointsToTransform.push_back({&obj.relativePos, false});
				vectorsToTransform.push_back(&obj.vetorPointData["normal"]);
			} else if (obj.objType == "mesh") {
				for (auto& face : obj.facePoints) {
					for (auto& point : face) {
						pointsToTransform.push_back({&point, true});
					}
				}
			}

			for (const auto& t : obj.transforms) {
				for (auto& [point, isVertex] : pointsToTransform) {
					if (t.tType == "translation") {
						point->translate(t.data);
					} else if (t.tType == "scaling" && isVertex) {
						point->scale(t.data);
					} else if (t.tType == "rotation" && isVertex) {
						point->rotate(t.data);
					}
				}

				for (auto& vector : vectorsToTransform) {
					if (t.tType == "rotation") {
						vector->rotate(t.data);
					}
				}

				for (auto& scalar : scalarsToTransform) {
					if (t.tType == "scaling") {
						*scalar *= t.data.getX();  // Scale the radius by the
												   // maximum scaling factor
					}
				}
			}

			if (obj.objType == "mesh") {
				for (const auto& face : obj.facePoints) {
					obj.faceNormals.push_back(
						(face[1] - face[0])
							.normalized()
							.cross((face[2] - face[0]).normalized())
							.normalized());
				}
			}
		}
	}
};

#endif