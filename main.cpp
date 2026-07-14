#include <iostream>

#include "src/Camera.h"
#include "src/Octree.h"
#include "src/Ponto.h"
#include "src/Vetor.h"
#include "utils/Scene/sceneParser.cpp"

using namespace std;

struct arguments {
	string inputName;
	string sceneFile;
	string outputFile;
	bool oldMethod;
	bool debug;

	arguments() : inputName("utils/input/caso4.json"), sceneFile(""), outputFile("imagem"), oldMethod(false), debug(false) {}
};

arguments parseArguments(int argc, char* argv[]) {
	arguments args;
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];
		if (arg == "-i" && i + 1 < argc) {
			args.inputName = string("utils/input/") + argv[++i] + ".json";
		} else if (arg == "-f" && i + 1 < argc) {
			args.sceneFile = argv[++i];
			cout << "Scene file: " << args.sceneFile << endl;
		} else if (arg == "-o" && i + 1 < argc) {
			args.outputFile = argv[++i];
		} else if (arg == "--old") {
			args.oldMethod = true;
		} else if (arg == "--debug") {
			args.debug = true;
		}
	}
	return args;
}

struct Timer {
	std::chrono::time_point<std::chrono::high_resolution_clock> start;

	Timer() {
		start = std::chrono::high_resolution_clock::now();
	}

	~Timer() {
		double time = elapsed();
		std::cout << "Execution time: " << time << " seconds" << std::endl;
	}

	double elapsed() const {
		auto now = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double>(now - start).count();
	}
};

int main(int argc, char* argv[]) {
	Timer timer;  // Start the timer

	auto args = parseArguments(argc, argv);

	string sceneFile = args.sceneFile.empty() ? args.inputName : args.sceneFile;
	SceneData scene = SceneJsonLoader::loadFile(sceneFile);

	Camera camera(scene);

	if (args.debug) {
		cout << "DEBUG MODE" << endl;
		camera.renderAlt(scene.objects);
	}
	if (args.oldMethod) {
		cout << "OLD MODE" << endl;
		camera.renderOld(scene.objects);
	} else {
		cout << "NEW MODE" << endl;
		camera.render(scene.objects);
	}

	camera.plotPixels(args.outputFile);
}