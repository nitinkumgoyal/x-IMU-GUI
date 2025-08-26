from typing import Dict, Any, Optional, List, Callable
import time
import statistics
from dataclasses import dataclass, asdict

from smart_sleeve_system import ICommunicationInterface, SmartSleeveMessage
from sensor_interfaces import (
    MultiSensorController, SensorType, SensorReading, FluidMeasurement,
    PressureSensor, CapacitiveSensor, OpticalSensor, UltrasonicSensor, FlowRateSensor
)


@dataclass
class InjectionEvent:
    """Data structure for injection detection events"""
    timestamp: float
    detection_method: str
    triggering_sensors: List[str]
    volume_dispensed_ml: float
    confidence_level: float
    pressure_at_detection_kpa: float
    flow_rate_ml_per_sec: float
    sensor_readings: List[SensorReading]


class SensorFusion:
    """Advanced sensor fusion for improved accuracy and reliability"""
    
    def __init__(self):
        self.sensor_weights = {
            SensorType.PRESSURE: 0.4,      # High weight for direct pressure measurement
            SensorType.FLOW_RATE: 0.3,     # High weight for direct flow measurement
            SensorType.CAPACITIVE: 0.15,   # Medium weight for volume estimation
            SensorType.OPTICAL: 0.1,       # Lower weight for presence detection
            SensorType.ULTRASONIC: 0.05    # Lower weight due to potential interference
        }
        self.confidence_thresholds = {
            'high': 0.8,
            'medium': 0.6,
            'low': 0.4
        }
        
    def fuse_volume_measurements(self, sensor_readings: List[SensorReading], 
                                syringe_config: Dict[str, float]) -> FluidMeasurement:
        """Fuse multiple sensor readings to get best volume estimate"""
        
        volume_estimates = []
        pressure_reading = None
        flow_rate_reading = None
        
        # Process each sensor reading
        for reading in sensor_readings:
            if reading.sensor_type == SensorType.PRESSURE.value:
                pressure_reading = reading
                # Estimate volume change from pressure (requires calibration)
                volume_change = self._pressure_to_volume(reading.primary_value, syringe_config)
                volume_estimates.append((volume_change, self.sensor_weights[SensorType.PRESSURE]))
                
            elif reading.sensor_type == SensorType.CAPACITIVE.value:
                # Direct volume measurement from capacitance
                volume = reading.primary_value * syringe_config.get('capacitive_factor', 0.1)
                volume_estimates.append((volume, self.sensor_weights[SensorType.CAPACITIVE]))
                
            elif reading.sensor_type == SensorType.ULTRASONIC.value:
                # Volume from liquid level
                diameter = syringe_config.get('diameter_mm', 10.0)
                volume = self._level_to_volume(reading.primary_value, diameter)
                volume_estimates.append((volume, self.sensor_weights[SensorType.ULTRASONIC]))
                
            elif reading.sensor_type == SensorType.FLOW_RATE.value:
                flow_rate_reading = reading
                # Flow rate doesn't directly give volume, but helps with confidence
                
            elif reading.sensor_type == SensorType.OPTICAL.value:
                # Optical mainly for presence detection, contributes to confidence
                pass
        
        # Calculate weighted average volume
        if volume_estimates:
            total_weight = sum(weight for _, weight in volume_estimates)
            if total_weight > 0:
                weighted_volume = sum(vol * weight for vol, weight in volume_estimates) / total_weight
            else:
                weighted_volume = 0.0
        else:
            weighted_volume = 0.0
        
        # Calculate confidence based on sensor agreement and quality
        confidence = self._calculate_fusion_confidence(sensor_readings, volume_estimates)
        
        # Create comprehensive measurement
        return FluidMeasurement(
            timestamp=time.time(),
            initial_volume_ml=syringe_config.get('initial_volume', 0.0),
            current_volume_ml=max(0.0, syringe_config.get('initial_volume', 0.0) - weighted_volume),
            dispensed_volume_ml=weighted_volume,
            flow_rate_ml_per_sec=flow_rate_reading.primary_value if flow_rate_reading else 0.0,
            pressure_kpa=pressure_reading.primary_value if pressure_reading else 0.0,
            confidence_level=confidence,
            sensor_readings=sensor_readings,
            measurement_method="sensor_fusion"
        )
    
    def _pressure_to_volume(self, pressure_kpa: float, syringe_config: Dict[str, float]) -> float:
        """Convert pressure reading to volume change estimate"""
        # This would be calibrated based on syringe mechanics and fluid properties
        pressure_to_volume_factor = syringe_config.get('pressure_volume_factor', 0.05)
        return max(0.0, pressure_kpa * pressure_to_volume_factor)
    
    def _level_to_volume(self, level_mm: float, diameter_mm: float) -> float:
        """Convert liquid level to volume"""
        if level_mm <= 0:
            return 0.0
        
        import math
        radius_mm = diameter_mm / 2
        volume_mm3 = math.pi * radius_mm * radius_mm * level_mm
        return volume_mm3 / 1000.0  # Convert to mL
    
    def _calculate_fusion_confidence(self, readings: List[SensorReading], 
                                   volume_estimates: List[tuple]) -> float:
        """Calculate confidence level based on sensor agreement and quality"""
        if not readings:
            return 0.0
        
        # Average sensor quality
        avg_quality = statistics.mean(reading.quality for reading in readings)
        
        # Volume estimate agreement (if multiple estimates available)
        if len(volume_estimates) >= 2:
            volumes = [vol for vol, _ in volume_estimates]
            volume_std = statistics.stdev(volumes) if len(volumes) > 1 else 0.0
            volume_mean = statistics.mean(volumes)
            
            # Lower standard deviation relative to mean indicates better agreement
            agreement_factor = 1.0 - min(1.0, volume_std / (volume_mean + 0.1))
        else:
            agreement_factor = 0.8  # Moderate confidence with single sensor
        
        # Sensor diversity bonus
        unique_sensor_types = len(set(reading.sensor_type for reading in readings))
        diversity_bonus = min(0.2, unique_sensor_types * 0.05)
        
        # Combine factors
        confidence = (avg_quality * 0.5 + agreement_factor * 0.4 + diversity_bonus)
        return min(1.0, max(0.0, confidence))
    
    def detect_injection_event(self, sensor_readings: List[SensorReading], 
                             previous_readings: List[SensorReading],
                             thresholds: Dict[str, float]) -> Optional[InjectionEvent]:
        """Detect injection events using multiple sensor modalities"""
        
        detection_indicators = []
        triggering_sensors = []
        
        current_time = time.time()
        
        # Check each sensor type for injection indicators
        for reading in sensor_readings:
            if reading.sensor_type == SensorType.PRESSURE.value:
                # Pressure increase indicates injection start
                pressure_threshold = thresholds.get('pressure_kpa', 2.0)
                if reading.primary_value > pressure_threshold:
                    detection_indicators.append(('pressure_increase', 0.9))
                    triggering_sensors.append(reading.sensor_id)
            
            elif reading.sensor_type == SensorType.FLOW_RATE.value:
                # Flow rate above threshold indicates active injection
                flow_threshold = thresholds.get('flow_rate_ml_per_s', 0.1)
                if reading.primary_value > flow_threshold:
                    detection_indicators.append(('flow_detected', 0.95))
                    triggering_sensors.append(reading.sensor_id)
            
            elif reading.sensor_type == SensorType.CAPACITIVE.value:
                # Capacitance decrease indicates volume reduction
                if previous_readings:
                    prev_cap = next((r.primary_value for r in previous_readings 
                                   if r.sensor_type == SensorType.CAPACITIVE.value), 0)
                    cap_change = prev_cap - reading.primary_value
                    if cap_change > thresholds.get('capacitance_change', 1.0):
                        detection_indicators.append(('volume_decrease', 0.7))
                        triggering_sensors.append(reading.sensor_id)
            
            elif reading.sensor_type == SensorType.OPTICAL.value:
                # Optical change indicates liquid movement
                optical_threshold = thresholds.get('optical_change', 0.1)
                if previous_readings:
                    prev_optical = next((r.primary_value for r in previous_readings 
                                       if r.sensor_type == SensorType.OPTICAL.value), 1.0)
                    optical_change = abs(prev_optical - reading.primary_value)
                    if optical_change > optical_threshold:
                        detection_indicators.append(('optical_change', 0.6))
                        triggering_sensors.append(reading.sensor_id)
        
        # Determine if injection event occurred
        if detection_indicators:
            # Calculate overall confidence
            total_confidence = sum(conf for _, conf in detection_indicators)
            num_indicators = len(detection_indicators)
            avg_confidence = total_confidence / num_indicators if num_indicators > 0 else 0.0
            
            # Require minimum confidence for detection
            if avg_confidence >= 0.6:  # Configurable threshold
                # Get volume and flow rate estimates
                volume_ml = 0.0
                flow_rate = 0.0
                pressure_kpa = 0.0
                
                for reading in sensor_readings:
                    if reading.sensor_type == SensorType.FLOW_RATE.value:
                        flow_rate = reading.primary_value
                    elif reading.sensor_type == SensorType.PRESSURE.value:
                        pressure_kpa = reading.primary_value
                
                return InjectionEvent(
                    timestamp=current_time,
                    detection_method="multi_sensor_fusion",
                    triggering_sensors=triggering_sensors,
                    volume_dispensed_ml=volume_ml,
                    confidence_level=avg_confidence,
                    pressure_at_detection_kpa=pressure_kpa,
                    flow_rate_ml_per_sec=flow_rate,
                    sensor_readings=sensor_readings
                )
        
        return None


class EnhancedSmartSleeveController:
    """Enhanced smart sleeve controller with multi-sensor support and fusion"""
    
    def __init__(self, device_id: str, communication_interface: ICommunicationInterface):
        self.device_id = device_id
        self.comm_interface = communication_interface
        self.sensor_controller = MultiSensorController(device_id)
        self.sensor_fusion = SensorFusion()
        
        self.is_monitoring = False
        self.injection_thresholds = {
            'pressure_kpa': 2.0,
            'flow_rate_ml_per_s': 0.1,
            'capacitance_change': 1.0,
            'optical_change': 0.1
        }
        
        self.syringe_config = {
            'initial_volume': 10.0,    # mL
            'diameter_mm': 10.0,       # mm
            'capacitive_factor': 0.1,  # pF to mL conversion
            'pressure_volume_factor': 0.05  # kPa to mL conversion
        }
        
        self.callbacks: Dict[str, Callable] = {}
        self.previous_readings: List[SensorReading] = []
        self.measurement_history: List[FluidMeasurement] = []
        
    def add_sensor(self, sensor_id: str, sensor_type: SensorType, 
                   config: Optional[Dict[str, Any]] = None) -> bool:
        """Add a sensor to the smart sleeve"""
        return self.sensor_controller.add_sensor(sensor_id, sensor_type, config)
    
    def setup_multi_sensor_configuration(self, sensor_config: Dict[str, Dict[str, Any]]) -> bool:
        """Setup multiple sensors at once"""
        success = True
        
        for sensor_id, config in sensor_config.items():
            sensor_type_str = config.get('type', '').upper()
            try:
                sensor_type = SensorType[sensor_type_str]
                sensor_params = config.get('parameters', {})
                
                if not self.add_sensor(sensor_id, sensor_type, sensor_params):
                    print(f"Failed to add sensor: {sensor_id}")
                    success = False
                    
            except KeyError:
                print(f"Unknown sensor type: {sensor_type_str}")
                success = False
        
        return success
    
    def initialize_system(self, comm_config: Dict[str, Any], hub_address: str, 
                         sensor_configs: Optional[Dict[str, Dict[str, Any]]] = None) -> bool:
        """Initialize the complete enhanced smart sleeve system"""
        try:
            # Initialize communication
            if not self.comm_interface.initialize(comm_config):
                return False
            
            # Connect to hub
            if not self.comm_interface.connect(hub_address):
                return False
            
            # Setup sensors if provided
            if sensor_configs:
                if not self.setup_multi_sensor_configuration(sensor_configs):
                    print("Warning: Some sensors failed to initialize")
            
            # Calibrate all sensors
            if not self.sensor_controller.calibrate_all_sensors():
                print("Warning: Some sensors failed calibration")
            
            # Send initialization message
            init_message = SmartSleeveMessage(
                message_type="enhanced_system_init",
                device_id=self.device_id,
                timestamp=time.time(),
                data={
                    "status": "initialized",
                    "sensor_count": len(self.sensor_controller.sensors),
                    "sensor_types": list(self.sensor_controller.get_sensor_status().keys()),
                    "fusion_enabled": True
                },
                protocol_used=comm_config.get('protocol', 'unknown')
            )
            
            return self.comm_interface.send_message(init_message)
            
        except Exception as e:
            print(f"Enhanced system initialization failed: {e}")
            return False
    
    def start_monitoring(self, syringe_volume_ml: float = 10.0) -> bool:
        """Start comprehensive monitoring with all sensors"""
        try:
            self.syringe_config['initial_volume'] = syringe_volume_ml
            self.is_monitoring = True
            
            # Get initial readings from all sensors
            initial_readings = self.sensor_controller.get_all_readings()
            self.previous_readings = initial_readings
            
            # Create initial measurement
            initial_measurement = self.sensor_fusion.fuse_volume_measurements(
                initial_readings, self.syringe_config
            )
            self.measurement_history.append(initial_measurement)
            
            # Send monitoring start message
            message = SmartSleeveMessage(
                message_type="monitoring_started",
                device_id=self.device_id,
                timestamp=time.time(),
                data={
                    "initial_measurement": asdict(initial_measurement),
                    "sensor_readings": [asdict(r) for r in initial_readings],
                    "syringe_config": self.syringe_config
                },
                protocol_used=self.comm_interface.__class__.__name__
            )
            
            self.comm_interface.send_message(message)
            
            if "monitoring_started" in self.callbacks:
                self.callbacks["monitoring_started"](initial_measurement)
            
            return True
            
        except Exception as e:
            print(f"Failed to start monitoring: {e}")
            return False
    
    def monitor_injection(self) -> Optional[FluidMeasurement]:
        """Enhanced injection monitoring with sensor fusion"""
        if not self.is_monitoring:
            return None
        
        try:
            # Get current readings from all sensors
            current_readings = self.sensor_controller.get_all_readings()
            
            # Perform sensor fusion for volume measurement
            current_measurement = self.sensor_fusion.fuse_volume_measurements(
                current_readings, self.syringe_config
            )
            
            # Check for injection events
            injection_event = self.sensor_fusion.detect_injection_event(
                current_readings, self.previous_readings, self.injection_thresholds
            )
            
            if injection_event:
                self._handle_injection_event(injection_event, current_measurement)
            
            # Update history
            self.measurement_history.append(current_measurement)
            if len(self.measurement_history) > 100:  # Keep last 100 measurements
                self.measurement_history.pop(0)
            
            self.previous_readings = current_readings
            
            return current_measurement
            
        except Exception as e:
            print(f"Injection monitoring error: {e}")
            return None
    
    def _handle_injection_event(self, injection_event: InjectionEvent, 
                               measurement: FluidMeasurement):
        """Handle detected injection event"""
        # Send injection detection message to Connected OR HUB
        message = SmartSleeveMessage(
            message_type="injection_detected_enhanced",
            device_id=self.device_id,
            timestamp=time.time(),
            data={
                "injection_event": asdict(injection_event),
                "current_measurement": asdict(measurement),
                "trigger_quantification": True,
                "sensor_fusion_confidence": measurement.confidence_level
            },
            protocol_used=self.comm_interface.__class__.__name__
        )
        
        self.comm_interface.send_message(message)
        
        if "injection_detected" in self.callbacks:
            self.callbacks["injection_detected"](injection_event, measurement)
    
    def get_sensor_status(self) -> Dict[str, Any]:
        """Get comprehensive sensor and system status"""
        sensor_status = self.sensor_controller.get_sensor_status()
        
        return {
            "device_id": self.device_id,
            "is_monitoring": self.is_monitoring,
            "is_connected": self.comm_interface.is_connected(),
            "sensor_count": len(self.sensor_controller.sensors),
            "sensors": sensor_status,
            "signal_strength": self.comm_interface.get_signal_strength(),
            "measurement_history_count": len(self.measurement_history),
            "fusion_enabled": True
        }
    
    def register_callback(self, event_type: str, callback: Callable):
        """Register callbacks for events"""
        self.callbacks[event_type] = callback
    
    def configure_injection_thresholds(self, thresholds: Dict[str, float]):
        """Configure thresholds for injection detection"""
        self.injection_thresholds.update(thresholds)
    
    def configure_syringe(self, syringe_config: Dict[str, float]):
        """Configure syringe parameters"""
        self.syringe_config.update(syringe_config)
    
    def get_measurement_history(self, count: int = 10) -> List[FluidMeasurement]:
        """Get recent measurement history"""
        return self.measurement_history[-count:] if self.measurement_history else []
    
    def shutdown(self):
        """Gracefully shutdown the enhanced system"""
        self.is_monitoring = False
        
        # Send shutdown message
        message = SmartSleeveMessage(
            message_type="enhanced_system_shutdown",
            device_id=self.device_id,
            timestamp=time.time(),
            data={
                "reason": "normal_shutdown",
                "total_measurements": len(self.measurement_history),
                "sensor_status": self.sensor_controller.get_sensor_status()
            },
            protocol_used=self.comm_interface.__class__.__name__
        )
        
        self.comm_interface.send_message(message)
        self.comm_interface.disconnect()