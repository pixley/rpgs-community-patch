using UnityModManagerNet;
using HarmonyLib;
using System.Reflection;

namespace RpgsCommunityPatch
{
    static class Main
    {
        private static UnityModManager.ModEntry mod;
        private static Harmony harmony;

        // Entry point for the mod.
        static bool Load(UnityModManager.ModEntry modEntry)
        {
            harmony = new Harmony(modEntry.Info.Id);
            mod = modEntry;

            mod.OnToggle = OnToggle;

            return true;
        }

        // Controls enable/disable
        static bool OnToggle(UnityModManager.ModEntry modEntry, bool value)
        {
            if (value)
            {
                OnEnable();
            }
            else
            {
                OnDisable();
            }

            return true;
        }

        static void OnEnable()
        {
            string version = GetVersionString();
            Log($"RPG Sounds Community Patch version {version} loaded.");

            harmony.PatchAll(Assembly.GetExecutingAssembly());
        }

        static void OnDisable()
        {
            Log("RPG Sounds Community Patch disabled.");

            harmony.UnpatchAll(harmony.Id);
        }

        public static string GetVersionString()
        {
            // ModEntry contains the version info pulled from Info.json
            return mod.Version.ToString();
        }

        public static void Log(string logString)
        {
            mod.Logger.Log(logString);
        }
    }
}
