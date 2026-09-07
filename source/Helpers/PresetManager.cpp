#include "PresetManager.h"
#include "../PluginProcessor.h"

using namespace juce;

PresetManager::PresetManager (AudioProcessor* processor)
    : mProcessor (processor)
{
    loadPresetDirectorySettings();
    ensurePresetDirectoryExists();
    storeLocalPresets();
}

PresetManager::~PresetManager()
{
    savePresetDirectorySettings();
}

String PresetManager::getPresetDirectory() const
{
    return mPresetDirectory;
}

bool PresetManager::setPresetDirectory (const String& directory)
{
    File dir (directory);
    if (dir.isDirectory())
    {
        mPresetDirectory = directory;
        savePresetDirectorySettings();
        ensurePresetDirectoryExists();
        storeLocalPresets();
        return true;
    }
    return false;
}

void PresetManager::setCrtEnabled (bool enabled)
{
    mCrtEnabled = enabled;
    savePresetDirectorySettings();
}

void PresetManager::setCrtStrength (int strength)
{
    mCrtStrength = jlimit (0, 2, strength);
    savePresetDirectorySettings();
}

void PresetManager::resetExportDirectoryToDefault()
{
    mExportDirectory = File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Babel")
        .getChildFile ("Exports")
        .getFullPathName();
    savePresetDirectorySettings();
    File dir (mExportDirectory);
    if (! dir.isDirectory())
        dir.createDirectory();
}

juce::String PresetManager::getExportDirectory() const
{
    return mExportDirectory;
}

bool PresetManager::setExportDirectory (const String& directory)
{
    File dir (directory);
    if (dir.isDirectory())
    {
        mExportDirectory = directory;
        savePresetDirectorySettings();
        return true;
    }
    return false;
}

void PresetManager::resetPresetDirectoryToDefault()
{
    mPresetDirectory = File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Babel")
        .getChildFile ("Presets")
        .getFullPathName();
    savePresetDirectorySettings();
    ensurePresetDirectoryExists();
    storeLocalPresets();
}

void PresetManager::ensurePresetDirectoryExists()
{
    File dir (mPresetDirectory);
    if (! dir.isDirectory())
        dir.createDirectory();
}

File PresetManager::getSettingsFile() const
{
    return File (mPresetDirectory).getParentDirectory().getChildFile ("settings.xml");
}

File PresetManager::getBookmarkFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Babel")
        .getChildFile ("settings.xml");
}

void PresetManager::loadPresetDirectorySettings()
{
    String storedDir;
    String storedExport;
    {
        File bookmarkFile = getBookmarkFile();
        if (bookmarkFile.existsAsFile())
        {
            std::unique_ptr<XmlElement> xml (XmlDocument::parse (bookmarkFile));
            if (xml != nullptr)
            {
                storedDir = xml->getStringAttribute ("presetDirectory", "");
                storedExport = xml->getStringAttribute ("exportDirectory", "");
                mCrtEnabled = xml->getBoolAttribute ("crtEnabled", mCrtEnabled);
                mCrtStrength = jlimit (0, 2, xml->getIntAttribute ("crtStrength", mCrtStrength));
            }
        }
    }

    if (storedDir.isEmpty())
    {
        resetPresetDirectoryToDefault();
        if (mExportDirectory.isEmpty())
            resetExportDirectoryToDefault();
        return;
    }

    mPresetDirectory = storedDir;
    mExportDirectory = storedExport;

    mLastSettingsFile = getSettingsFile();
    if (mLastSettingsFile.existsAsFile())
    {
        std::unique_ptr<XmlElement> xml (XmlDocument::parse (mLastSettingsFile));
        if (xml != nullptr)
        {
            const String dir = xml->getStringAttribute ("presetDirectory", "");
            if (! dir.isEmpty())
            {
                mPresetDirectory = dir;
                mLastSettingsFile = getSettingsFile();
            }
            const String exportDir = xml->getStringAttribute ("exportDirectory", "");
            if (! exportDir.isEmpty())
                mExportDirectory = exportDir;
            mCrtEnabled = xml->getBoolAttribute ("crtEnabled", mCrtEnabled);
            mCrtStrength = jlimit (0, 2, xml->getIntAttribute ("crtStrength", mCrtStrength));
        }
    }
    if (mExportDirectory.isEmpty())
        resetExportDirectoryToDefault();
}

void PresetManager::savePresetDirectorySettings()
{
    File settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    XmlElement xml ("BabelSettings");
    xml.setAttribute ("presetDirectory", mPresetDirectory);
    xml.setAttribute ("exportDirectory", mExportDirectory);
    xml.setAttribute ("crtEnabled", mCrtEnabled);
    xml.setAttribute ("crtStrength", mCrtStrength);

    xml.writeTo (settingsFile);

    File bookmarkFile = getBookmarkFile();
    if (bookmarkFile != settingsFile)
    {
        bookmarkFile.getParentDirectory().createDirectory();
        xml.writeTo (bookmarkFile);
    }

    if (mLastSettingsFile.existsAsFile()
        && mLastSettingsFile != settingsFile
        && mLastSettingsFile != bookmarkFile)
        mLastSettingsFile.deleteFile();

    mLastSettingsFile = settingsFile;
}

void PresetManager::storeLocalPresets()
{
    mLocalPresets.clear();
    File dir (mPresetDirectory);
    if (dir.isDirectory())
    {
        for (auto& entry : RangedDirectoryIterator (dir, true, "*" + String (BABEL_PRESET_EXTENSION)))
            mLocalPresets.add (entry.getFile());
        mLocalPresets.sort();
    }
}

int PresetManager::getNumberOfPresets()
{
    return mLocalPresets.size();
}

String PresetManager::getPresetName (int index)
{
    if (isPositiveAndBelow (index, mLocalPresets.size()))
        return mLocalPresets[index].getFileNameWithoutExtension();
    return {};
}

File PresetManager::getPresetFile (int index) const
{
    if (isPositiveAndBelow (index, mLocalPresets.size()))
        return mLocalPresets[index];
    return {};
}

String PresetManager::getCurrentPresetName()
{
    return mCurrentPresetName;
}

void PresetManager::createNewPreset()
{
    mCurrentPresetName = "Untitled";
    mIsSaved = false;
    mCurrentlyLoadedPreset = File();

    for (auto* param : mProcessor->getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

void PresetManager::savePreset()
{
    if (mCurrentlyLoadedPreset.existsAsFile())
    {
        MemoryBlock block;
        mProcessor->getStateInformation (block);
        mCurrentlyLoadedPreset.replaceWithData (block.getData(), block.getSize());
        mIsSaved = true;
    }
    else
    {
        saveAsPreset (mCurrentPresetName);
    }
}

void PresetManager::saveAsPreset (const String& name)
{
    const String legalName = File::createLegalFileName (name).trim();
    if (legalName.isEmpty() || legalName == "." || legalName == "..")
        return;

    mCurrentPresetName = legalName;
    File presetFile = File (mPresetDirectory).getChildFile (legalName + BABEL_PRESET_EXTENSION);

    MemoryBlock block;
    mProcessor->getStateInformation (block);
    presetFile.replaceWithData (block.getData(), block.getSize());

    mCurrentlyLoadedPreset = presetFile;
    mIsSaved = true;
    storeLocalPresets();
}

bool PresetManager::loadPreset (int index)
{
    if (! isPositiveAndBelow (index, mLocalPresets.size()))
        return false;

    return loadPresetFile (mLocalPresets[index]);
}

bool PresetManager::loadPresetFile (const File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;

    MemoryBlock block;
    if (presetFile.loadFileAsData (block))
    {
        mProcessor->setStateInformation (block.getData(), (int) block.getSize());
        mCurrentPresetName = presetFile.getFileNameWithoutExtension();
        mCurrentlyLoadedPreset = presetFile;
        mIsSaved = true;
        return true;
    }
    return false;
}
