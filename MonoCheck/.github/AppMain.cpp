#include <juce_audio_utils/juce_audio_utils.h>
#include "MonoCheckEngine.h"
#include "MonoCheckView.h"

namespace
{
    // Buffer de entrada. Con 512 muestras (~11 ms a 48 kHz) el análisis cuesta una fracción mínima de CPU
    // y no pide un buffer más pequeño de lo necesario. Se puede cambiar en Ajustes (engranaje).
    constexpr int defaultBufferSize = 512;
    const char* const versionLabel  = "TxuSo  v1.8";
}

//==============================================================================
class AppMainComponent : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit AppMainComponent (juce::ApplicationProperties& propsToUse)
        : props (propsToUse),
          callback (engine),
          view (engine)
    {
        // Interfaz (mismo cuadro y medidor que el plugin), con engranaje de ajustes
        view.setVersionText (versionLabel);
        view.setRefreshRate (30);
        view.onGearClicked = [this] { showSettings(); };

        const juce::String savedKey = props.getUserSettings()->getValue ("resetKey", "R");
        if (savedKey.isNotEmpty())
            view.setResetKey (savedKey[0]);

        view.onResetKeyChanged = [this] (juce::juce_wchar c)
        {
            props.getUserSettings()->setValue ("resetKey", juce::String::charToString (c));
            props.saveIfNeeded();
        };

        addAndMakeVisible (view);
        setSize (240, 384);

        // Audio: solo entrada (0 salidas). Se recuerda el dispositivo y los canales elegidos.
        std::unique_ptr<juce::XmlElement> saved = props.getUserSettings()->getXmlValue ("audioDeviceState");
        const bool firstRun = (saved == nullptr);

        deviceManager.initialise (2, 0, saved.get(), true);

        if (firstRun)
        {
            auto setup = deviceManager.getAudioDeviceSetup();
            setup.bufferSize = defaultBufferSize;
            deviceManager.setAudioDeviceSetup (setup, true);
        }

        deviceManager.addAudioCallback (&callback);
        deviceManager.addChangeListener (this);
    }

    ~AppMainComponent() override
    {
        deviceManager.removeChangeListener (this);
        deviceManager.removeAudioCallback (&callback);
        saveAudioState();
    }

    void resized() override
    {
        view.setBounds (getLocalBounds());
    }

    // ⌘ , abre los ajustes (atajo habitual en Mac)
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == ',' && key.getModifiers().isCommandDown())
        {
            showSettings();
            return true;
        }

        return false;
    }

private:
    //==========================================================================
    // Recibe el audio de la entrada elegida y se lo pasa al motor. No reproduce nada.
    class Callback : public juce::AudioIODeviceCallback
    {
    public:
        explicit Callback (MonoCheckEngine& e) : engine (e) {}

        void audioDeviceAboutToStart (juce::AudioIODevice* device) override
        {
            engine.prepare (device->getCurrentSampleRate());
        }

        void audioDeviceStopped() override {}

        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                               float* const* outputChannelData, int numOutputChannels,
                                               int numSamples,
                                               const juce::AudioIODeviceCallbackContext&) override
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (outputChannelData[ch] != nullptr)
                    juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

            if (numInputChannels >= 2 && inputChannelData[0] != nullptr && inputChannelData[1] != nullptr)
                engine.process (inputChannelData[0], inputChannelData[1], numSamples);
            else if (numInputChannels == 1 && inputChannelData[0] != nullptr)
                engine.process (inputChannelData[0], inputChannelData[0], numSamples);
            else
                engine.processSilence (numSamples);
        }

    private:
        MonoCheckEngine& engine;
    };

    //==========================================================================
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        saveAudioState();          // se guarda la entrada elegida para el próximo arranque
    }

    void saveAudioState()
    {
        if (auto xml = deviceManager.createStateXml())
            props.getUserSettings()->setValue ("audioDeviceState", xml.get());

        props.saveIfNeeded();
    }

    // Ajustes: lista todas las entradas de audio del sistema y sus canales, para elegir cuáles analizar
    void showSettings()
    {
        auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                                 0, 2,      // entradas: se analiza un par estéreo
                                                                 0, 0,      // sin salidas
                                                                 false, false,
                                                                 true,      // canales como pares estéreo
                                                                 false);    // opciones avanzadas visibles (buffer, frecuencia)
        selector->setSize (480, 360);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (selector);
        options.dialogTitle = "Ajustes de entrada de audio";
        options.componentToCentreAround = this;
        options.dialogBackgroundColour = juce::Colour (0xff2a2a2e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    //==========================================================================
    juce::ApplicationProperties& props;
    MonoCheckEngine engine;
    juce::AudioDeviceManager deviceManager;
    Callback callback;
    MonoCheckView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppMainComponent)
};

//==============================================================================
class MonoCheckApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "MonoCheck"; }
    const juce::String getApplicationVersion() override { return "1.8.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        // Ajustes guardados en ~/Library/Application Support/MonoCheck
        juce::PropertiesFile::Options options;
        options.applicationName     = "MonoCheck";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "MonoCheck";
        options.storageFormat       = juce::PropertiesFile::storeAsXML;
        props.setStorageParameters (options);

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), props);
    }

    void shutdown() override            { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::ApplicationProperties& p)
            : DocumentWindow (name, juce::Colour (0xff131316), DocumentWindow::allButtons),
              props (p)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new AppMainComponent (props), true);
            setResizable (true, true);
            setResizeLimits (160, 240, 900, 1400);

            // Recuerda tamaño y posición de la ventana
            const juce::String saved = props.getUserSettings()->getValue ("windowState");

            if (saved.isNotEmpty())
                restoreWindowStateFromString (saved);
            else
                centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        ~MainWindow() override
        {
            props.getUserSettings()->setValue ("windowState", getWindowStateAsString());
            props.saveIfNeeded();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        juce::ApplicationProperties& props;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    juce::ApplicationProperties props;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (MonoCheckApplication)
