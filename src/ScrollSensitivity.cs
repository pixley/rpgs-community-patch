using HarmonyLib;
using UnityEngine.UI;
using System.Reflection;

namespace RpgsCommunityPatch
{
    [HarmonyPatch(typeof(LoopScrollRect), "OnScroll")]
    public static class ScrollSensitivity
    {
        private static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                Main.Log("Applied scroll sensitivity fix.");
            }
        }

        private static void Prefix(LoopScrollRect __instance)
        {
            // By default, the Library's scroll sensitivity is 2, which is awful
            // 40 was chosen based on feel
            __instance.scrollSensitivity = 40f;
        }
    }
}
