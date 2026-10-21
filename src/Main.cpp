#include "RunwayPlugin.h"

static RunwayPlugin* g_plugin = nullptr;

void __declspec(dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
    *ppPlugInInstance = g_plugin = new RunwayPlugin();
}

void __declspec(dllexport) EuroScopePlugInExit(void)
{
    delete g_plugin;
    g_plugin = nullptr;
}
