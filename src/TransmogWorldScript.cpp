#include "Transmog.h"

class TransmogWorldScript : public WorldScript
{
public:
    TransmogWorldScript() : WorldScript("TransmogWorldScript", {
        WORLDHOOK_ON_STARTUP
    }) { }

// Load configuration and remove slot rows for characters no longer in the database.
    void OnStartup() override
    {
        sTransmog->LoadConfig();
        // Runs before the world loop handles logins, so the unlock data needs no lock.
        sTransmog->LoadUnlockedAppearances();

        CharacterDatabase.Execute("DELETE FROM mod_transmog_plus WHERE NOT EXISTS (SELECT 1 FROM characters WHERE characters.guid = mod_transmog_plus.Owner)");
    }
};

void AddSC_TransmogWorldScript()
{
    new TransmogWorldScript();
}
