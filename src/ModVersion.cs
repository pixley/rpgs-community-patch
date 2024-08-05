using HarmonyLib;
using System.Reflection;

namespace RpgsCommunityPatch
{
    [HarmonyPatch(typeof(ViewAppState), "Start")]
    public static class ModVersion
    {
        private static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                Main.Log("Applied community patch info to version shown in UI.");
            }
        }

        private static void Postfix(ViewAppState __instance)
        {
            Traverse versionField = Traverse.Create(__instance).Field("txtVersion");
            TMPro.TextMeshProUGUI versionText = (TMPro.TextMeshProUGUI)(versionField.GetValue());

            // This overrides the txtVersion private member that is used to write version info
            // in the top-left corner of the application
            versionText.enableWordWrapping = false;
            versionText.text = versionText.text + " + Community Patch v" + Main.GetVersionString();
        }
    }
}
