from abc import ABC, abstractmethod
from enum import Enum
from typing import Dict, Any, Optional, List, Tuple
import time
import math
from dataclasses import dataclass


class SensorType(Enum):
    """Supported sensor types for fluid detection and measurement"""
    CAPACITIVE = "capacitive"
    PRESSURE = "pressure"
    OPTICAL = "optical"
    ULTRASONIC = "ultrasonic"
    LOAD_CELL = "load_cell"
    FLOW_RATE = "flow_rate"
    TEMPERATURE = "temperature"
    PROXIMITY = "proximity"


@dataclass
class SensorReading:
    """Generic sensor reading with metadata"""
    timestamp: float
    sensor_type: str
    primary_value: float
    secondary_value: Optional[float] = None
    unit: str = ""
    quality: float = 1.0  # 0.0 to 1.0, quality/confidence of reading
    temperature: float = 25.0  # Sensor temperature for compensation
    device_id: str = ""
    sensor_id: str = ""


@dataclass
class FluidMeasurement:
    """Comprehensive fluid measurement data from multiple sensors"""
    timestamp: float
    initial_volume_ml: float
    current_volume_ml: float
    dispensed_volume_ml: float
    flow_rate_ml_per_sec: float
    pressure_kpa: float
    confidence_level: float
    sensor_readings: List[SensorReading]
    measurement_method: str


class ISensorInterface(ABC):
    """Abstract interface for all sensor types"""
    
    @abstractmethod
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize the sensor with specific configuration"""
        pass
    
    @abstractmethod
    def calibrate(self) -> bool:
        """Calibrate the sensor"""
        pass
    
    @abstractmethod
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current sensor reading"""
        pass
    
    @abstractmethod
    def get_sensor_type(self) -> SensorType:
        """Get the sensor type"""
        pass
    
    @abstractmethod
    def is_calibrated(self) -> bool:
        """Check if sensor is calibrated"""
        pass
    
    @abstractmethod
    def reset(self) -> bool:
        """Reset sensor to initial state"""
        pass
    
    @abstractmethod
    def self_test(self) -> bool:
        """Perform sensor self-test"""
        pass


class PressureSensor(ISensorInterface):
    """Pressure sensor interface for tip-based fluid detection"""
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_pressure = 0.0
        self.sensitivity = self.config.get('sensitivity', 1.0)
        self.pressure_range_kpa = self.config.get('pressure_range_kpa', (0, 100))
        self.filter_enabled = self.config.get('filter_enabled', True)
        self.recent_readings = []
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize pressure sensor"""
        try:
            self.config.update(config)
            self.sensitivity = config.get('sensitivity', 1.0)
            self.pressure_range_kpa = config.get('pressure_range_kpa', (0, 100))
            
            # Initialize pressure sensor hardware
            # In real implementation: configure ADC, pressure transducer, etc.
            print(f"Pressure sensor initialized - Range: {self.pressure_range_kpa} kPa")
            return True
            
        except Exception as e:
            print(f"Pressure sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        """Calibrate pressure sensor with ambient pressure"""
        try:
            # Take multiple readings for baseline
            readings = []
            for _ in range(10):
                readings.append(self._read_raw_pressure())
                time.sleep(0.01)
            
            self.baseline_pressure = sum(readings) / len(readings)
            self.is_calibrated_flag = True
            
            print(f"Pressure sensor calibrated - Baseline: {self.baseline_pressure:.2f} kPa")
            return True
            
        except Exception as e:
            print(f"Pressure sensor calibration failed: {e}")
            return False
    
    def _read_raw_pressure(self) -> float:
        """Read raw pressure value from sensor hardware"""
        # Simulate pressure reading with realistic values
        # In real implementation: read from pressure transducer via ADC
        import random
        
        # Simulate pressure changes during injection
        base_pressure = 101.325  # Atmospheric pressure in kPa
        injection_pressure = random.uniform(0, 50)  # Additional pressure during injection
        noise = random.uniform(-0.1, 0.1)
        
        return base_pressure + injection_pressure + noise
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current pressure reading"""
        raw_pressure = self._read_raw_pressure()
        
        # Apply calibration offset
        calibrated_pressure = raw_pressure - self.baseline_pressure if self.is_calibrated_flag else raw_pressure
        
        # Apply filtering if enabled
        if self.filter_enabled:
            calibrated_pressure = self._apply_filter(calibrated_pressure)
        
        # Calculate quality based on noise level and sensor stability
        quality = self._calculate_reading_quality(calibrated_pressure)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type=self.get_sensor_type().value,
            primary_value=calibrated_pressure,
            secondary_value=raw_pressure,
            unit="kPa",
            quality=quality,
            temperature=self._get_sensor_temperature(),
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _apply_filter(self, pressure: float) -> float:
        """Apply low-pass filter to reduce noise"""
        self.recent_readings.append(pressure)
        if len(self.recent_readings) > 5:
            self.recent_readings.pop(0)
        
        # Simple moving average filter
        return sum(self.recent_readings) / len(self.recent_readings)
    
    def _calculate_reading_quality(self, pressure: float) -> float:
        """Calculate quality/confidence of the pressure reading"""
        # Quality based on sensor range utilization and stability
        range_min, range_max = self.pressure_range_kpa
        
        if pressure < range_min or pressure > range_max:
            return 0.3  # Out of range
        
        # Higher quality for readings in optimal range (middle 60%)
        optimal_range = (range_max - range_min) * 0.6
        optimal_center = (range_max + range_min) / 2
        
        distance_from_optimal = abs(pressure - optimal_center)
        if distance_from_optimal <= optimal_range / 2:
            return 1.0
        else:
            return max(0.5, 1.0 - (distance_from_optimal / (range_max - range_min)))
    
    def _get_sensor_temperature(self) -> float:
        """Get sensor temperature for compensation"""
        # Simulate temperature reading
        import random
        return random.uniform(20, 30)
    
    def detect_injection_start(self, threshold_kpa: float = 2.0) -> bool:
        """Detect start of injection based on pressure increase"""
        if not self.is_calibrated_flag:
            return False
        
        reading = self.get_reading("", "")
        return reading.primary_value > threshold_kpa
    
    def detect_injection_end(self, threshold_kpa: float = 1.0) -> bool:
        """Detect end of injection based on pressure decrease"""
        if not self.is_calibrated_flag:
            return False
        
        reading = self.get_reading("", "")
        return reading.primary_value < threshold_kpa
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.PRESSURE
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        """Reset sensor to initial state"""
        self.is_calibrated_flag = False
        self.baseline_pressure = 0.0
        self.recent_readings.clear()
        return True
    
    def self_test(self) -> bool:
        """Perform sensor self-test"""
        try:
            # Test sensor responsiveness
            reading1 = self._read_raw_pressure()
            time.sleep(0.1)
            reading2 = self._read_raw_pressure()
            
            # Check if sensor is responsive (readings should be similar but not identical)
            diff = abs(reading1 - reading2)
            return 0.01 <= diff <= 10.0  # Reasonable pressure variation
            
        except Exception:
            return False


class CapacitiveSensor(ISensorInterface):
    """Enhanced capacitive sensor for liquid volume detection"""
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_capacitance = 0.0
        self.volume_calibration_factor = self.config.get('volume_factor', 0.1)
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize capacitive sensor"""
        try:
            self.config.update(config)
            self.volume_calibration_factor = config.get('volume_factor', 0.1)
            
            print("Capacitive sensor initialized")
            return True
            
        except Exception as e:
            print(f"Capacitive sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        """Calibrate with empty syringe"""
        try:
            readings = []
            for _ in range(10):
                readings.append(self._read_raw_capacitance())
                time.sleep(0.01)
            
            self.baseline_capacitance = sum(readings) / len(readings)
            self.is_calibrated_flag = True
            
            print(f"Capacitive sensor calibrated - Baseline: {self.baseline_capacitance:.2f}")
            return True
            
        except Exception as e:
            print(f"Capacitive sensor calibration failed: {e}")
            return False
    
    def _read_raw_capacitance(self) -> float:
        """Read raw capacitance value"""
        import random
        return random.uniform(100.0, 500.0)
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current capacitance reading"""
        raw_capacitance = self._read_raw_capacitance()
        calibrated_value = raw_capacitance - self.baseline_capacitance if self.is_calibrated_flag else raw_capacitance
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type=self.get_sensor_type().value,
            primary_value=calibrated_value,
            secondary_value=raw_capacitance,
            unit="pF",
            quality=0.9,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def calculate_volume(self, capacitance_reading: float) -> float:
        """Calculate volume from capacitance reading"""
        return max(0.0, capacitance_reading * self.volume_calibration_factor)
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.CAPACITIVE
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_capacitance = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class OpticalSensor(ISensorInterface):
    """Optical sensor for liquid level and flow detection"""
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_intensity = 0.0
        self.sensor_wavelength = self.config.get('wavelength_nm', 850)  # Near-infrared
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize optical sensor"""
        try:
            self.config.update(config)
            self.sensor_wavelength = config.get('wavelength_nm', 850)
            
            print(f"Optical sensor initialized - Wavelength: {self.sensor_wavelength}nm")
            return True
            
        except Exception as e:
            print(f"Optical sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        """Calibrate with empty syringe"""
        try:
            readings = []
            for _ in range(10):
                readings.append(self._read_light_intensity())
                time.sleep(0.01)
            
            self.baseline_intensity = sum(readings) / len(readings)
            self.is_calibrated_flag = True
            
            print(f"Optical sensor calibrated - Baseline: {self.baseline_intensity:.2f}")
            return True
            
        except Exception as e:
            print(f"Optical sensor calibration failed: {e}")
            return False
    
    def _read_light_intensity(self) -> float:
        """Read light intensity from photodetector"""
        import random
        # Simulate light intensity changes due to liquid presence
        return random.uniform(0.1, 1.0)
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current optical reading"""
        intensity = self._read_light_intensity()
        
        # Calculate transmission/absorption
        transmission = intensity / self.baseline_intensity if self.is_calibrated_flag and self.baseline_intensity > 0 else intensity
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type=self.get_sensor_type().value,
            primary_value=transmission,
            secondary_value=intensity,
            unit="ratio",
            quality=0.95,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def detect_liquid_presence(self, threshold: float = 0.8) -> bool:
        """Detect liquid presence based on light transmission"""
        reading = self.get_reading("", "")
        return reading.primary_value < threshold  # Less transmission = liquid present
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.OPTICAL
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_intensity = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class UltrasonicSensor(ISensorInterface):
    """Ultrasonic sensor for liquid level measurement"""
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.empty_distance_mm = 0.0
        self.frequency_khz = self.config.get('frequency_khz', 40)
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize ultrasonic sensor"""
        try:
            self.config.update(config)
            self.frequency_khz = config.get('frequency_khz', 40)
            
            print(f"Ultrasonic sensor initialized - Frequency: {self.frequency_khz}kHz")
            return True
            
        except Exception as e:
            print(f"Ultrasonic sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        """Calibrate with empty syringe"""
        try:
            readings = []
            for _ in range(10):
                readings.append(self._measure_distance())
                time.sleep(0.01)
            
            self.empty_distance_mm = sum(readings) / len(readings)
            self.is_calibrated_flag = True
            
            print(f"Ultrasonic sensor calibrated - Empty distance: {self.empty_distance_mm:.2f}mm")
            return True
            
        except Exception as e:
            print(f"Ultrasonic sensor calibration failed: {e}")
            return False
    
    def _measure_distance(self) -> float:
        """Measure distance to liquid surface"""
        import random
        # Simulate distance measurement (shorter distance = more liquid)
        return random.uniform(5.0, 50.0)
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current distance reading"""
        distance = self._measure_distance()
        
        # Calculate liquid level
        liquid_level = self.empty_distance_mm - distance if self.is_calibrated_flag else 0
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type=self.get_sensor_type().value,
            primary_value=liquid_level,
            secondary_value=distance,
            unit="mm",
            quality=0.85,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def calculate_volume_from_level(self, level_mm: float, syringe_diameter_mm: float) -> float:
        """Calculate volume from liquid level"""
        if level_mm <= 0:
            return 0.0
        
        radius_mm = syringe_diameter_mm / 2
        volume_mm3 = math.pi * radius_mm * radius_mm * level_mm
        return volume_mm3 / 1000.0  # Convert to mL
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.ULTRASONIC
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.empty_distance_mm = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class FlowRateSensor(ISensorInterface):
    """Flow rate sensor for measuring fluid dispensing speed"""
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.calibration_factor = self.config.get('calibration_factor', 1.0)
        self.recent_flows = []
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize flow rate sensor"""
        try:
            self.config.update(config)
            self.calibration_factor = config.get('calibration_factor', 1.0)
            
            print("Flow rate sensor initialized")
            return True
            
        except Exception as e:
            print(f"Flow rate sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        """Calibrate flow rate sensor"""
        try:
            # Flow rate sensors typically don't need baseline calibration
            self.is_calibrated_flag = True
            print("Flow rate sensor calibrated")
            return True
            
        except Exception as e:
            print(f"Flow rate sensor calibration failed: {e}")
            return False
    
    def _measure_flow_rate(self) -> float:
        """Measure current flow rate"""
        import random
        # Simulate flow rate measurement (0 when not injecting, positive during injection)
        return random.uniform(0, 5.0)  # mL/s
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        """Get current flow rate reading"""
        flow_rate = self._measure_flow_rate() * self.calibration_factor
        
        # Keep recent readings for averaging
        self.recent_flows.append(flow_rate)
        if len(self.recent_flows) > 10:
            self.recent_flows.pop(0)
        
        average_flow = sum(self.recent_flows) / len(self.recent_flows)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type=self.get_sensor_type().value,
            primary_value=flow_rate,
            secondary_value=average_flow,
            unit="mL/s",
            quality=0.9,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def is_flowing(self, threshold_ml_per_s: float = 0.1) -> bool:
        """Detect if fluid is currently flowing"""
        reading = self.get_reading("", "")
        return reading.primary_value > threshold_ml_per_s
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.FLOW_RATE
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.recent_flows.clear()
        return True
    
    def self_test(self) -> bool:
        return True


class SensorFactory:
    """Factory for creating sensor instances"""
    
    @staticmethod
    def create_sensor(sensor_type: SensorType, config: Optional[Dict[str, Any]] = None) -> ISensorInterface:
        """Create sensor instance based on type"""
        if sensor_type == SensorType.PRESSURE:
            return PressureSensor(config)
        elif sensor_type == SensorType.CAPACITIVE:
            return CapacitiveSensor(config)
        elif sensor_type == SensorType.OPTICAL:
            return OpticalSensor(config)
        elif sensor_type == SensorType.ULTRASONIC:
            return UltrasonicSensor(config)
        elif sensor_type == SensorType.FLOW_RATE:
            return FlowRateSensor(config)
        else:
            raise ValueError(f"Unsupported sensor type: {sensor_type}")


class MultiSensorController:
    """Controller for managing multiple sensors simultaneously"""
    
    def __init__(self, device_id: str):
        self.device_id = device_id
        self.sensors: Dict[str, ISensorInterface] = {}
        self.sensor_configs: Dict[str, Dict[str, Any]] = {}
        
    def add_sensor(self, sensor_id: str, sensor_type: SensorType, config: Optional[Dict[str, Any]] = None) -> bool:
        """Add a sensor to the controller"""
        try:
            sensor = SensorFactory.create_sensor(sensor_type, config)
            
            if sensor.initialize(config or {}):
                self.sensors[sensor_id] = sensor
                self.sensor_configs[sensor_id] = config or {}
                print(f"Added {sensor_type.value} sensor: {sensor_id}")
                return True
            else:
                print(f"Failed to initialize {sensor_type.value} sensor: {sensor_id}")
                return False
                
        except Exception as e:
            print(f"Failed to add sensor {sensor_id}: {e}")
            return False
    
    def calibrate_all_sensors(self) -> bool:
        """Calibrate all sensors"""
        success = True
        for sensor_id, sensor in self.sensors.items():
            if not sensor.calibrate():
                print(f"Failed to calibrate sensor: {sensor_id}")
                success = False
        return success
    
    def get_all_readings(self) -> List[SensorReading]:
        """Get readings from all sensors"""
        readings = []
        for sensor_id, sensor in self.sensors.items():
            try:
                reading = sensor.get_reading(self.device_id, sensor_id)
                readings.append(reading)
            except Exception as e:
                print(f"Failed to read from sensor {sensor_id}: {e}")
        return readings
    
    def get_sensor_reading(self, sensor_id: str) -> Optional[SensorReading]:
        """Get reading from specific sensor"""
        if sensor_id in self.sensors:
            try:
                return self.sensors[sensor_id].get_reading(self.device_id, sensor_id)
            except Exception as e:
                print(f"Failed to read from sensor {sensor_id}: {e}")
        return None
    
    def perform_self_tests(self) -> Dict[str, bool]:
        """Perform self-tests on all sensors"""
        results = {}
        for sensor_id, sensor in self.sensors.items():
            results[sensor_id] = sensor.self_test()
        return results
    
    def get_sensor_status(self) -> Dict[str, Dict[str, Any]]:
        """Get status of all sensors"""
        status = {}
        for sensor_id, sensor in self.sensors.items():
            status[sensor_id] = {
                'type': sensor.get_sensor_type().value,
                'calibrated': sensor.is_calibrated(),
                'self_test_passed': sensor.self_test()
            }
        return status
    
    def remove_sensor(self, sensor_id: str) -> bool:
        """Remove a sensor from the controller"""
        if sensor_id in self.sensors:
            del self.sensors[sensor_id]
            if sensor_id in self.sensor_configs:
                del self.sensor_configs[sensor_id]
            print(f"Removed sensor: {sensor_id}")
            return True
        return False