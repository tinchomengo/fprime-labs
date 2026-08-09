module TelemetryDeployment {

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

  instance imuSim: ImuAltitudeVisualization.ImuSim base id 0x10015000

  instance imuReader: ImuAltitudeVisualization.ImuReader base id 0x10016000

  instance uartDriver: Drv.LinuxUartDriver base id 0x10017000

  @ Supplies the receive buffers uartDriver needs. One bin is enough here:
  @ every MPU6050 line is well under 64 bytes, and a handful of buffers
  @ gives it a little slack while a line is being processed.
  instance uartBufferManager: Svc.BufferManager base id 0x10018000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::BufferManager::BufferBins bins;
    """

    phase Fpp.ToCpp.Phases.configComponents """
    memset(&ConfigObjects::TelemetryDeployment_uartBufferManager::bins, 0, sizeof(ConfigObjects::TelemetryDeployment_uartBufferManager::bins));
    ConfigObjects::TelemetryDeployment_uartBufferManager::bins.bins[0].bufferSize = 64;
    ConfigObjects::TelemetryDeployment_uartBufferManager::bins.bins[0].numBuffers = 4;
    TelemetryDeployment::uartBufferManager.setup(
        1,
        0,
        TelemetryDeployment::mallocator,
        ConfigObjects::TelemetryDeployment_uartBufferManager::bins
    );
    """

    phase Fpp.ToCpp.Phases.tearDownComponents """
    TelemetryDeployment::uartBufferManager.cleanup();
    """
  }

}
