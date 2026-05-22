#include "app/Application.h"

int main(int argc, char* argv[])
{
    StudioLog::Application app(argc, argv);
    return app.exec();
}
