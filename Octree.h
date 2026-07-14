#ifndef OCTREEHEADER
#define OCTREEHEADER

#include <array>
#include <queue>
#include <vector>

#include "../utils/Scene/sceneSchema.hpp"

class Node;	   // Forward declaration of Node class
class Object;  // Forward declaration of Object class

using Matrix = std::vector<std::vector<double>>;

double infinity = std::numeric_limits<double>::max();

double sqr(double value) {
	return value * value;
}

struct Ray {
	Ponto origin;
	Vetor direction;
	double distance;

	Ray() : distance(infinity) {}
	Ray(Ponto o, Vetor dir) : origin(o), direction(dir), distance(infinity) {}
	Ray(Ray& ray, double dist) : origin(ray.origin), direction(ray.direction), distance(dist) {}
	Ray(Ponto o, Vetor dir, double max) : origin(o), direction(dir), distance(max) {}

	Ray apply(Matrix& matrix) {
		return {
			this->origin.applied(matrix),
			this->direction.applied(matrix),
			this->distance};
	}
};

struct Collision {
	bool hit;
	double distance;
	Node* node;	 // Pointer to the node that was hit (if any)

	Collision(bool hit) : hit(false), distance(infinity), node(nullptr) {}
	Collision(bool hit, double distance, Node* node) : hit(hit), distance(distance), node(node) {}

	bool operator<(const Collision& other) const {
		return distance > other.distance;  // For priority queue (min-heap)
	}
};

struct Intersection {
	bool hit;			 // Indicates if the ray hit an object or not
	double distance;	 // Distance from ray origin to the hit point
	Vetor normal;		 // The normal vector at the hit point, used for lighting calculations
	ObjectData* object;	 // Pointer to the object that was hit

	Intersection() : hit(false), distance(infinity), object(nullptr) {}
	Intersection(bool hit) : hit(false), distance(infinity), object(nullptr) {}
	Intersection(double dist) : hit(false), distance(dist), object(nullptr) {}
	Intersection(bool hit, double distance, Vetor normal, ObjectData* object)
		: hit(hit), distance(distance), normal(normal), object(object) {}

	operator bool() const {
		return this->hit;
	}

	bool operator<(const Intersection& that) const {
		return this->distance < that.distance;
	}
};

struct Heap {
	priority_queue<Collision> collisions;

	Heap() {}
	Heap(Collision& collision) {
		collisions.push(collision);
	}

	int size() {
		return collisions.size();
	}

	void push(Collision& collision) {
		collisions.push(collision);
	}

	Collision pop() {
		Collision top = collisions.top();
		collisions.pop();
		return top;
	}
};

struct Bounds {
	Ponto min;	// Minimum boundary of the octree (corner of the bounding box)
	Ponto max;	// Maximum boundary of the octree (opposite corner of the bounding box)

	Bounds() {}
	Bounds(Ponto min, Ponto max) : min(min), max(max) {}

	static bool collides(const Bounds& box, const Bounds& bounds) {
		bool partX = (box.min.x <= bounds.max.x) && (box.max.x >= bounds.min.x);
		bool partY = (box.min.y <= bounds.max.y) && (box.max.y >= bounds.min.y);
		bool partZ = (box.min.z <= bounds.max.z) && (box.max.z >= bounds.min.z);

		return partX && partY && partZ;
	}

	static Bounds merge(const Bounds& a, const Bounds& b) {
		Ponto newMin = Ponto::min(a.min, b.min);
		Ponto newMax = Ponto::max(a.max, b.max);
		return Bounds(newMin, newMax);
	}

	static Bounds Infinity() {
		return Bounds(Ponto(-infinity, -infinity, -infinity), Ponto(infinity, infinity, infinity));
	}
};

struct Object {
	Ponto center;		 // Center of the object
	ObjectData* object;	 // Pointer to the object data (sphere or mesh)

	Object(ObjectData* obj) : object(obj), center(obj->relativePos) {}
	virtual ~Object() = default;

	virtual Bounds getBounds() = 0;								// Pure virtual function to get the bounds of the object
	virtual Intersection intersect(Ray& ray, double dist) = 0;	// Pure virtual function for intersection logic

	virtual bool collides(Bounds& bounds) {
		Bounds box = this->getBounds();
		return Bounds::collides(box, bounds);
	}
};

struct Plane : public Object {
	Vetor normal;  // Normal vector of the plane

	Plane(ObjectData* obj) : Object(obj), normal(obj->vetorPointData["normal"].normalized()) {}

	bool collides(Bounds& bounds) override {
		return false;  // Plane is never inside a octree
	}

	Bounds getBounds() override {
		return Bounds::Infinity();
	}

	Intersection intersect(Ray& ray, double dist) override {
		ray.distance = dist;
		auto& [origin, direction, maxDistance] = ray;
		double dotNorm = direction.dot(normal);

		// Ray is parallel to the plane, no intersection
		if (fabs(dotNorm) == 0) return false;
		double t = (this->center - origin).dot(normal) / dotNorm;

		// Only consider hits in front of the camera
		if (t > 1e-4 && t < maxDistance) {
			return {true, t, normal, this->object};
		}

		return false;  // No intersection
	}
};

struct Sphere : public Object {
	double radius;	// Radius of the sphere

	Sphere(ObjectData* obj, double r) : Object(obj), radius(r) {}

	bool collides(Bounds& bounds) override {
		Ponto closest = Ponto::max(bounds.min, Ponto::min(this->center, bounds.max));
		Vetor distance = closest - this->center;
		return distance.scale() < this->radius * this->radius;
	}

	Bounds getBounds() override {
		Ponto min = this->center - radius;
		Ponto max = this->center + radius;
		return Bounds(min, max);
	}

	Intersection intersect(Ray& ray, double dist) override {
		ray.distance = dist;
		auto& [origin, direction, maxDistance] = ray;
		Vetor oc = origin - center;
		double A = direction.dot(direction);
		double B = 2.0 * oc.dot(direction);
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
			if (t > 1e-4 && t < maxDistance) {
				auto normal = (origin + direction * t - center).normalized();
				return {true, t, normal, this->object};
			}
		}
		return false;
	}
};

struct Triangle : public Object {
	Vetor normal;			 // Normals for the face
	array<Ponto, 3> points;	 // List of vertices (Pontos)

	Triangle(ObjectData* obj, Vetor& normal, vector<Ponto>& face) : Object(obj), normal(normal) {
		points[0] = face[0] + obj->relativePos;
		points[1] = face[1] + obj->relativePos;
		points[2] = face[2] + obj->relativePos;
	}

	Bounds getBounds() override {
		Ponto minBox = Ponto::min({points[0], points[1], points[2]});
		Ponto maxBox = Ponto::max({points[0], points[1], points[2]});
		return Bounds(minBox, maxBox);
	}

	Intersection intersect(Ray& ray, double dist) override {
		ray.distance = dist;
		auto& [origin, direction, maxDistance] = ray;
		double dotNorm = direction.dot(normal);

		// Check for parallelism
		if (fabs(dotNorm) == 0) return false;  // Ray is parallel to triangle plane

		// Compute intersection t with the triangle plane
		double t = (points[0] - origin).dot(normal) / dotNorm;
		if (t < 0 || t > maxDistance) return false;	 // Must be in front of camera and closer than previous hit

		// Intersection point
		Ponto P = origin + direction * t;

		// Inside-triangle test using edge-cross tests
		Vetor C;
		C = (points[1] - points[0]).cross(P - points[0]);
		if (normal.dot(C) < 0) return false;
		C = (points[2] - points[1]).cross(P - points[1]);
		if (normal.dot(C) < 0) return false;
		C = (points[0] - points[2]).cross(P - points[2]);
		if (normal.dot(C) < 0) return false;

		// Passed all checks: it's inside the triangle
		return {true, t, normal, this->object};
	}
};

struct Cylinder : public Object {
	Vetor axis;	 // Axis of the cylinder
	Vetor across;
	Vetor forward;
	double radius;		// Radius of the cylinder
	double height;		// Height of the cylinder
	Matrix worldSpace;	// Matrix to convert from Local to World Space
	Matrix localSpace;	// Matrix to convert from World to Local Space

	Cylinder(ObjectData* obj, Vetor a, double r, double h) : Object(obj), axis(a.normalized()), radius(r), height(h) {
		Vetor P = this->center;

		Vetor temp;
		if (abs(axis.y) > 0.9) {
			temp = {0, 0, 1};
			across = axis.cross(temp);
			forward = across.cross(axis);
		} else {
			temp = {0, 1, 0};
			across = temp.cross(axis);
			forward = across.cross(axis);
		}

		Vetor X, Z, A;

		A = axis * h;
		X = across * r;
		Z = forward * r;
		worldSpace = {
			{X.x, A.x, Z.x, P.x},
			{X.y, A.y, Z.y, P.y},
			{X.z, A.z, Z.z, P.z},
			{0, 0, 0, 1},
		};

		A = axis / h;
		X = across / r;
		Z = forward / r;
		Vetor Q = {
			-(across.dot(P) / r),
			-(this->axis.dot(P) / h),
			-(forward.dot(P) / r),
		};

		localSpace = {
			{X.x, X.y, X.z, Q.x},
			{A.x, A.y, A.z, Q.y},
			{Z.x, Z.y, Z.z, Q.z},
			{0, 0, 0, 1},
		};
	}

	Bounds getBounds() override {
		Ponto base = this->center;
		Ponto top = this->center + axis * height;

		Ponto limits = {
			this->radius * sqrt(1 - axis.x * axis.x),
			this->radius * sqrt(1 - axis.y * axis.y),
			this->radius * sqrt(1 - axis.z * axis.z)};

		Ponto min = Ponto::min(base, top) - limits;
		Ponto max = Ponto::max(base, top) + limits;
		return {min, max};
	}

	Intersection intersect(Ray& ray, double dist) override {
		auto localRay = ray.apply(localSpace);

		auto capsHit = this->cylinderCaps(localRay, dist);
		auto wallsHit = this->cylinderWalls(localRay, dist);

		if (capsHit < wallsHit)
			return capsHit;
		else
			return wallsHit;
	}

   private:
	Intersection cylinderWalls(Ray& ray, double dist) {
		ray.distance = dist;
		auto& [origin, direction, distance] = ray;
		double A = sqr(direction.x) + sqr(direction.z);
		double B = 2 * (origin.x * direction.x + origin.z * direction.z);
		double C = sqr(origin.x) + sqr(origin.z) - 1;

		double delta = sqr(B) - 4 * A * C;
		if (delta < 0) return false;

		delta = sqrt(delta);
		double t1 = (-B - delta) / (2 * A);
		double t2 = (-B + delta) / (2 * A);

		auto i1 = this->validateWall(ray, t1);
		auto i2 = this->validateWall(ray, t2);

		if (i1 < i2)
			return i1;
		else
			return i2;
	}

	Intersection validateWall(Ray& ray, double dist) {
		auto& [origin, direction, distance] = ray;
		if (dist < 0) return false;
		if (dist > distance) return false;

		Ponto point = origin + direction * dist;
		if (point.y < 0) return false;
		if (point.y > 1) return false;

		Vetor normal = (across * point.x + forward * point.z).normalized();
		// Vetor normal = Vetor(point.x, 0, point.z);
		// normal.apply(worldSpace);
		// normal.normalize();

		return {true, dist, normal, this->object};
	}

	Intersection cylinderCaps(Ray& ray, double dist) {
		auto& [origin, direction, distance] = ray;

		if (direction.y == 0) return false;

		double t1 = -origin.y / direction.y;
		double t2 = (1 - origin.y) / direction.y;

		auto i1 = this->validateCaps(ray, t1, {0, -1, 0});
		auto i2 = this->validateCaps(ray, t2, {0, 1, 0});

		if (i1 < i2) {
			return i1;
		} else {
			return i2;
		}
	}

	Intersection validateCaps(Ray& ray, double dist, Vetor normal) {
		auto& [origin, direction, distance] = ray;
		if (dist < 0) return false;
		if (dist > distance) return false;

		Ponto point = origin + direction * dist;

		double value = sqr(point.x) + sqr(point.z);
		if (value > 1) return false;

		normal.apply(worldSpace);
		normal.normalize();

		return {true, dist, normal, this->object};
	}
};

class Node {
	// * Types
   public:
	friend class Octree;

	// * Properties
   private:
	int depth;				   // Depth of the octree node
	bool isLeaf;			   // Flag to indicate if the node is a leaf
	Bounds bounds;			   // Bounds of the node (min and max points)
	array<Node*, 8>* octants;  // Pointers to the 8 child nodes (only for non-leaf nodes)
	vector<Object*>* objects;  // List of objects in this node (only for leaf nodes)

	// * Methods
   public:
	Node(int d = 0) : isLeaf(true), octants(nullptr), objects(new vector<Object*>()), depth(d) {}
	~Node() {
		if (octants) {
			for (auto octant : *octants) {
				delete octant;
			}
			delete octants;
		}
	}

	bool collides(Object* obj) {
		return obj->collides(bounds);
	}

	Collision collides(Ray& ray, double dist) {
		ray.distance = dist;
		auto& [origin, direction, distance] = ray;
		Ponto part1 = (bounds.min - origin) / direction;
		Ponto part2 = (bounds.max - origin) / direction;

		Ponto near = Ponto::min(part1, part2);
		Ponto far = Ponto::max(part1, part2);

		double entry = std::max({near.x, near.y, near.z});
		double exit = std::min({far.x, far.y, far.z});

		if (entry > exit) return false;		 // No intersection
		if (exit <= 0) return false;		 // The box is behind the ray
		if (entry > distance) return false;	 // The intersection is farther than the closest hit

		return {true, entry, this};
	}

	Intersection intersect(Ray& ray, double dist) {
		ray.distance = dist;
		auto& [origin, direction, distance] = ray;
		if (!isLeaf) return false;	// Only leaf nodes can contain objects

		Intersection hit(distance);

		for (auto obj : *objects) {
			Intersection result = obj->intersect(ray, hit.distance);
			if (result.hit && result.distance < hit.distance) {
				hit = result;
			}
		}

		return hit;
	}

	Bounds insert(ObjectData& object) {
		if (object.objType == "sphere") {
			Sphere* sphere = new Sphere(&object, object.getNum("radius"));

			objects->push_back(sphere);
			return sphere->getBounds();
		} else if (object.objType == "mesh") {
			Bounds meshBounds = Bounds::Infinity();
			for (int i = 0; i < object.facePoints.size(); ++i) {
				Triangle* triangle = new Triangle({&object, object.faceNormals[i], object.facePoints[i]});
				objects->push_back(triangle);
				meshBounds = Bounds::merge(meshBounds, triangle->getBounds());
			}
			return meshBounds;
		} else if (object.objType == "cylinder") {
			Cylinder* cylinder = new Cylinder(&object, object.getVetor("axis"), object.getNum("radius"), object.getNum("height"));

			objects->push_back(cylinder);
			return cylinder->getBounds();
		}

		return Bounds::Infinity();	// For unsupported object types, return an infinite bounds
	}

	void insert(Object* obj) {
		objects->push_back(obj);
	}

	void subdivide(int maxDepth) {
		if (!isLeaf) return;			   // Already subdivided
		if (depth >= maxDepth) return;	   // Reached maximum depth
		if (objects->size() <= 4) return;  // No need to subdivide if there's 0 or 1 object

		isLeaf = false;
		octants = new array<Node*, 8>();

		Ponto center = (bounds.min + bounds.max) * 0.5;

		for (int i = 0; i < 8; ++i) {
			Ponto newMin = bounds.min;
			Ponto newMax = center;

			if (i & 4) newMin.setX(center.x), newMax.setX(bounds.max.x);
			if (i & 2) newMin.setY(center.y), newMax.setY(bounds.max.y);
			if (i & 1) newMin.setZ(center.z), newMax.setZ(bounds.max.z);

			(*octants)[i] = new Node(depth + 1);
			(*octants)[i]->bounds = Bounds(newMin, newMax);
		}

		for (auto octant : *octants) {
			for (auto obj : *objects) {
				if (octant->collides(obj)) {
					octant->insert(obj);
				}
			}
		}
		delete objects;	 // Clear the objects from this node since they are now in the octants

		for (auto octant : *octants) {
			octant->subdivide(maxDepth);
		}
	}
};

class Octree {
	// * Properties
	Node* root;					// Pointer to the root node of the octree
	int maxDepth;				// Maximum depth of the octree
	Bounds bounds;				// Bounds of the octree (min and max points)
	vector<Plane> planes;		// List of objects in the octree
	vector<Cylinder> cylinder;	// List of objects in the octree

   public:
	~Octree() = default;
	Octree(int maxDepth = 10) : maxDepth(maxDepth) {
		this->root = new Node();  // Initialize the root as a leaf node
	}

	void insert(vector<ObjectData>& objects) {
		for (auto& object : objects) {
			this->insert(object);
		}
	}

	void insert(ObjectData& object) {
		if (object.objType == "plane") {
			planes.push_back(Plane(&object));
			return;	 // Planes are not inserted into the octree
		}
		if (object.objType == "cylinder") {
			cylinder.push_back(Cylinder(&object, object.getVetor("axis"), object.getNum("radius"), object.getNum("height")));
			return;	 // Planes are not inserted into the octree
		}

		auto updated = root->insert(object);
		bounds = Bounds::merge(bounds, updated);
	}

	void build(int maximumDepth = -1) {
		Vetor distance = bounds.max - bounds.min;

		double maxDistance = std::max({distance.getX(), distance.getY(), distance.getZ()});
		bounds.max = bounds.min + maxDistance;

		root->bounds = bounds;

		if (maximumDepth > 0) maxDepth = maximumDepth;
		root->subdivide(maxDepth);
	}

	Intersection raycast(Ray ray) {
		auto& [origin, direction, distance] = ray;
		Intersection hit(distance);

		for (auto& plane : planes) {
			Intersection check = plane.intersect(ray, hit.distance);
			if (check.hit) hit = check;
		}

		for (auto& plane : cylinder) {
			Intersection check = plane.intersect(ray, hit.distance);
			if (check.hit) hit = check;
		}

		Collision collision = root->collides(ray, hit.distance);

		if (!collision.hit) return hit;	 // No intersection with the octree
		Heap octants(collision);

		while (octants.size()) {
			collision = octants.pop();

			if (collision.node->isLeaf) {
				Intersection check = collision.node->intersect(ray, hit.distance);

				if (check.hit) return check;
			} else {
				for (auto octant : *collision.node->octants) {
					Collision newCollision = octant->collides(ray, hit.distance);
					if (newCollision.hit) {
						octants.push(newCollision);
					}
				}
			}
		}

		return hit;
	}
};

#endif	// OCTREEHEADER