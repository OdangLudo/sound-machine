#pragma once

#include "JuceHeader.h"
#include "GainProcessor.h"
#include "MixerChannelProcessor.h"
#include "SineBank.h"
#include "BalanceProcessor.h"
#include "InternalPluginFormat.h"

class ProcessorIds : private ChangeListener {
public:
    ProcessorIds() {
        PropertiesFile::Options options;
        options.applicationName = ProjectInfo::projectName;
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Preferences";
        appProperties.setStorageParameters(options);
        
        std::unique_ptr<XmlElement> savedPluginList(appProperties.getUserSettings()->getXmlValue(PLUGIN_LIST_FILE_NAME));

        if (savedPluginList != nullptr) {
            knownPluginListExternal.recreateFromXml(*savedPluginList);
        }
        
        InternalPluginFormat internalFormat;
        internalFormat.getAllTypes(internalTypes);
        for (auto* pluginType : internalTypes) {
            knownPluginListInternal.addType(*pluginType);
        }

        pluginSortMethod = (KnownPluginList::SortMethod) appProperties.getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByManufacturer);
        knownPluginListExternal.addChangeListener(this);

        formatManager.addDefaultFormats();
        formatManager.addFormat(new InternalPluginFormat());
    }

    ApplicationProperties& getApplicationProperties() {
        return appProperties;
    }

    PluginListComponent* makePluginListComponent() {
        const File &deadMansPedalFile = appProperties.getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
        return new PluginListComponent(formatManager, knownPluginListExternal, deadMansPedalFile, appProperties.getUserSettings(), true);
    }

    PluginDescription *getTypeForIdentifier(const String &identifier) {
        PluginDescription *description = knownPluginListInternal.getTypeForIdentifierString(identifier);
        return description != nullptr ? description : knownPluginListExternal.getTypeForIdentifierString(identifier);
    }
    
    KnownPluginList& getKnownPluginListExternal() {
        return knownPluginListExternal;
    }

    KnownPluginList& getKnownPluginListInternal() {
        return knownPluginListInternal;
    }

    const KnownPluginList::SortMethod getPluginSortMethod() const {
        return pluginSortMethod;
    }

    AudioPluginFormatManager& getFormatManager() {
        return formatManager;
    }

    void setPluginSortMethod(const KnownPluginList::SortMethod pluginSortMethod) {
        this->pluginSortMethod = pluginSortMethod;
    }

    const PluginDescription* getChosenType(const int menuId) const {
        PluginDescription *description = knownPluginListInternal.getType(knownPluginListInternal.getIndexChosenByMenu(menuId));
        return description != nullptr ? description : knownPluginListExternal.getType(knownPluginListExternal.getIndexChosenByMenu(menuId - knownPluginListInternal.getNumTypes()));
    }
    
private:
    const String PLUGIN_LIST_FILE_NAME = "pluginList";

    KnownPluginList knownPluginListExternal;
    KnownPluginList knownPluginListInternal;
    
    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;
    ApplicationProperties appProperties;
    OwnedArray<PluginDescription> internalTypes;

    void changeListenerCallback(ChangeBroadcaster* changed) override {
        if (changed == &knownPluginListExternal) {
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            std::unique_ptr<XmlElement> savedPluginList(knownPluginListExternal.createXml());

            if (savedPluginList != nullptr) {
                appProperties.getUserSettings()->setValue(PLUGIN_LIST_FILE_NAME, savedPluginList.get());
                appProperties.saveIfNeeded();
            }
        }
    }
};
