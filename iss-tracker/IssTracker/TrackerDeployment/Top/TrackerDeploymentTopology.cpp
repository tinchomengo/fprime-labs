// ======================================================================
// \title  TrackerDeploymentTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <IssTracker/TrackerDeployment/Top/TrackerDeploymentTopologyAc.hpp>
// Note: Uncomment when using Svc:TlmPacketizer
//#include <IssTracker/TrackerDeployment/Top/TrackerDeploymentPacketsAc.hpp>

// Necessary project-specified types
#include <Fw/Types/MallocAllocator.hpp>

#include <glob.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// Public functions for use in main program are namespaced with deployment module TrackerDeployment
// This is also the namespace where the topology components are instantiated by FPP.
namespace TrackerDeployment {

// Instantiate a malloc allocator for cmdSeq buffer allocation
Fw::MallocAllocator mallocator;

// The reference topology divides the incoming clock signal (1Hz) into sub-signals: 1Hz, 1/2Hz, and 1/4Hz with 0 offset
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
U32 rateGroup1Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
U32 rateGroup2Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
U32 rateGroup3Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};

enum TopologyConstants {
    COMM_PRIORITY = 34,
    UART_PRIORITY = 33,
};

// The Pico's USB serial port. Its name changes with the USB socket used
// (/dev/cu.usbmodem101, /dev/cu.usbmodem1101, ...), so it's found at
// startup. Set PICO_SERIAL_PORT to choose one explicitly.
static constexpr const char* PICO_SERIAL_ENV = "PICO_SERIAL_PORT";
static constexpr const char* PICO_SERIAL_PATTERN = "/dev/cu.usbmodem*";

//! Returns the Pico's serial port, or an empty string if none is found
static std::string findPicoSerialPort() {
    const char* fromEnvironment = std::getenv(PICO_SERIAL_ENV);
    if (fromEnvironment != nullptr && fromEnvironment[0] != '\0') {
        return fromEnvironment;
    }
    std::string found;
    glob_t matches;
    if (glob(PICO_SERIAL_PATTERN, 0, nullptr, &matches) == 0 && matches.gl_pathc > 0) {
        found = matches.gl_pathv[0];
        if (matches.gl_pathc > 1) {
            (void)printf("[PicoLink] Several %s ports found; using %s (set %s to choose)\n", PICO_SERIAL_PATTERN,
                         found.c_str(), PICO_SERIAL_ENV);
        }
    }
    globfree(&matches);
    return found;
}

//! Whether the serial port was opened, so its read thread is started and stopped
static bool s_uartOpened = false;

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);

    // Rate groups require context arrays.
    rateGroup1.configure(rateGroup1Context, FW_NUM_ARRAY_ELEMENTS(rateGroup1Context));
    rateGroup2.configure(rateGroup2Context, FW_NUM_ARRAY_ELEMENTS(rateGroup2Context));
    rateGroup3.configure(rateGroup3Context, FW_NUM_ARRAY_ELEMENTS(rateGroup3Context));

    // Command sequencer needs to allocate memory to hold contents of command sequences
    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);
}

void setupTopology(const TopologyState& state) {
    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();
    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded command registration. Function provided by autocoder.
    regCommands();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    if (state.hostname != nullptr && state.port != 0) {
        comDriver.configure(state.hostname, state.port);
    }

    // Open the Pico's USB serial port. It's a USB CDC virtual port, so the baud
    // rate has no effect, but termios still requires one. On success the driver
    // signals picoLink (drvConnected), which then starts the heartbeat.
    const std::string picoPort = findPicoSerialPort();
    if (picoPort.empty()) {
        (void)printf("[PicoLink] No %s port found: is the Pico plugged in? The mount won't move.\n",
                     PICO_SERIAL_PATTERN);
    } else {
        s_uartOpened = uartDriver.open(picoPort.c_str(), Drv::LinuxUartDriver::BAUD_115K,
                                       Drv::LinuxUartDriver::NO_FLOW, Drv::LinuxUartDriver::PARITY_NONE, 64);
        (void)printf("[PicoLink] %s %s\n", s_uartOpened ? "Opened" : "Could not open", picoPort.c_str());
    }

    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology();
    // Autocoded: reads PrmDb.dat into the parameter database. The generated
    // template only called loadParameters(), so saved parameters (such as the
    // observer location) were never read and every component used its defaults.
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);
    // Initialize socket communication if and only if there is a valid specification
    if (state.hostname != nullptr && state.port != 0) {
        Os::TaskString name("ReceiveTask");
        // Uplink is configured for receive so a socket task is started
        comDriver.start(name, COMM_PRIORITY, Default::STACK_SIZE);
    }
    if (s_uartOpened) {
        // Unlike Drv::TcpClient::start(), LinuxUartDriver::start() takes no task name
        uartDriver.start(UART_PRIORITY, Default::STACK_SIZE);
    }
}

void startRateGroups(const Fw::TimeInterval& interval) {
    // The timer component drives the fundamental tick rate of the system.
    // Svc::RateGroupDriver will divide this down to the slower rate groups.
    // This call will block until the stopRateGroups() call is made.
    // For this Linux demo, that call is made from a signal handler.
    timer.startTimer(interval);
}

void stopRateGroups() {
    timer.quit();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    // Other task clean-up.
    comDriver.stop();
    (void)comDriver.join();
    if (s_uartOpened) {
        uartDriver.quitReadThread();
        (void)uartDriver.join();
    }

    // Resource deallocation
    cmdSeq.deallocateBuffer(mallocator);

    tearDownComponents(state);
    deinitComponents(state);
}
};  // namespace TrackerDeployment
