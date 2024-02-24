using UnityEngine;
using UnityModManagerNet;
using HarmonyLib;
using System.Reflection;

namespace RpgsCommunityPatch
{
    static class Main
    {
        public static UnityModManager.ModEntry mod;

        // Entry point for the mod.
        static bool Load(UnityModManager.ModEntry modEntry)
        {
            mod = modEntry;

            mod.Logger.Log("Hello World!");

            string version = GetVersionString();
            mod.Logger.Log("RPG Sounds Community Patch version {version} loaded.");

            var harmony = new Harmony(modEntry.Info.Id);
            harmony.PatchAll(Assembly.GetExecutingAssembly());

            return true;
        }

        static string GetVersionString()
        {
            // ModEntry contains the version info pulled from Info.json
            return mod.Version.ToString();
        }
    }
}
