module TrackerDeployment {

  # ----------------------------------------------------------------------
  # Base ID Convention
  # ----------------------------------------------------------------------
  #
  # All Base IDs follow the 8-digit hex format: 0xDSSCCxxx
  #
  # Where:
  #   D   = Deployment digit (1 for this deployment)
  #   SS  = Subtopology digits (00 for main topology, 01-05 for subtopologies)
  #   CC  = Component digits (00, 01, 02, etc.)
  #   xxx = Reserved for internal component items (events, commands, telemetry)
  #

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    constant STACK_SIZE = 64 * 1024
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  instance rateGroup1: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 43

  instance rateGroup2: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 42

  instance rateGroup3: Svc.ActiveRateGroup base id 0x10003000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 41

  instance cmdSeq: Svc.CmdSequencer base id 0x10004000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 40

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------


  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance chronoTime: Svc.ChronoTime base id 0x10010000

  instance rateGroupDriver: Svc.RateGroupDriver base id 0x10011000

  instance systemResources: Svc.SystemResources base id 0x10012000

  instance timer: Svc.LinuxTimer base id 0x10013000

  instance comDriver: Drv.TcpClient base id 0x10014000

  instance sgp4Propagator: IssTracker.Sgp4Propagator base id 0x10015000

  instance antennaController: IssTracker.AntennaController base id 0x10016000

  instance picoLink: IssTracker.PicoLink base id 0x10017000

  @ USB serial to the Pico. Opened in TrackerDeploymentTopology.cpp.
  instance uartDriver: Drv.LinuxUartDriver base id 0x10018000

  @ Receive buffers for uartDriver. The Pico's lines are under 64 bytes;
  @ a few buffers give slack while one is being processed.
  instance uartBufferManager: Svc.BufferManager base id 0x10019000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::BufferManager::BufferBins bins;
    """

    phase Fpp.ToCpp.Phases.configComponents """
    memset(&ConfigObjects::TrackerDeployment_uartBufferManager::bins, 0, sizeof(ConfigObjects::TrackerDeployment_uartBufferManager::bins));
    ConfigObjects::TrackerDeployment_uartBufferManager::bins.bins[0].bufferSize = 64;
    ConfigObjects::TrackerDeployment_uartBufferManager::bins.bins[0].numBuffers = 8;
    TrackerDeployment::uartBufferManager.setup(
        1,
        0,
        TrackerDeployment::mallocator,
        ConfigObjects::TrackerDeployment_uartBufferManager::bins
    );
    """

    phase Fpp.ToCpp.Phases.tearDownComponents """
    TrackerDeployment::uartBufferManager.cleanup();
    """
  }

}
