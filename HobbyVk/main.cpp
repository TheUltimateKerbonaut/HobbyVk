#include <iostream>
#include "Renderer.h"

int main()
{
	Renderer renderer = Renderer(800, 600);

	while (renderer.ShouldRun())
	{
		renderer.PrepareFrame();
	}

	return 0;
}