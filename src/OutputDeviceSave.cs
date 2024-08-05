using System.Reflection;
using HarmonyLib;
using UnityEngine;

namespace RpgsCommunityPatch.src
{
    [HarmonyPatch(typeof(ViewOutputDeviceChanger), "OnDropdownChanged")]
    public static class OutputDeviceSave
    {
        private static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                Main.Log("Applied output device save fix.");

                // Shockingly, we don't actually have access to the app settings from anywhere in the
                // actual output device change flow
                appSettings = Object.FindObjectOfType<ApplicationSettings>();
                if (appSettings == null)
                {
                    Main.Log("ERROR: Could not locate app settings object!");
                    return;
                }
            }
        }

        private static void Postfix(int value)
        {
            if (appSettings != null)
            {
                appSettings.ChangeCurrentOutputDevice(value);
                Main.Log("Audio output device saved to settings.");
            }
        }

        private static ApplicationSettings appSettings = null;
    }

    [HarmonyPatch(typeof(ApplicationSettingsData), "Load")]
    public static class OutputDeviceLoad
    {
        private static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                // We can't just get these objects from the settings data, so we have to hunt for them
                deviceChanger = Object.FindObjectOfType<OutputDeviceChanger>();
                if (deviceChanger == null)
                {
                    Main.Log("ERROR: Could not locate output device changer object!");
                    return;
                }
            }
        }

        private static void Postfix(ApplicationSettingsData __instance)
        {
            if (deviceChanger == null)
            {
                return;
            }

            deviceChanger.SetCurrentDevice(__instance.currentOutputDeviceIndex);
            Main.Log("Loaded audio output device from settings.");
        }

        private static OutputDeviceChanger deviceChanger = null;
    }

    [HarmonyPatch(typeof(ViewOutputDeviceChanger), "OnEnable")]
    public static class UpdateDropdown
    {
        private static bool Prefix(ViewOutputDeviceChanger __instance)
        {
            // Loaded device selection doesn't propagate normally, so we need to force it
            Traverse instanceTraverse = Traverse.Create(__instance);

            OutputDeviceChangerConnector connector = instanceTraverse.Field("outputDeviceChangerConnector").GetValue<OutputDeviceChangerConnector>();
            connector.Controller.UpdateDeviceList();
            // Traverse.GetValue() executes the method
            // Push the options into the dropdown
            instanceTraverse.Method("OnListUpdated").GetValue();

            DropDown_TMP_Custom dropdown = instanceTraverse.Field("dropdown").GetValue<DropDown_TMP_Custom>();
            // Set the current selection
            Main.Log($"Number of options in dropdown now: {dropdown.options.Count}");
            Main.Log($"Double-checking current device index: {connector.Controller.CurrentDevice}");
            dropdown.value = connector.Controller.CurrentDevice;

            /*
            // Somehow the actual rendered text misses this, so we need to force the update here
            TMPro.TextMeshProUGUI[] textMeshChildren = __instance.gameObject.GetComponentsInChildren<TMPro.TextMeshProUGUI>();
            foreach (TMPro.TextMeshProUGUI child in textMeshChildren)
            {
                if (child.gameObject.name.EndsWith("Label"))
                {
                    Main.Log($"Found output device dropdown label!  Attempt to set to '{dropdown.captionText.text}'.");
                    child.text = dropdown.captionText.text;
                }
            }
            */

            return false;
        }
    }
}
