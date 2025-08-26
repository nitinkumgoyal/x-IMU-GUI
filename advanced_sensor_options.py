#!/usr/bin/env python3
"""
Advanced Sensor Options for Smart Sleeve System

This module explores additional sensor technologies beyond pressure sensors
for detecting fluid dispensing from syringes in medical applications.
"""

from abc import ABC, abstractmethod
from enum import Enum
from typing import Dict, Any, Optional, List, Tuple
import time
import math
from dataclasses import dataclass

from sensor_interfaces import ISensorInterface, SensorType, SensorReading


class AdvancedSensorType(Enum):
    """Advanced sensor types for fluid detection"""
    STRAIN_GAUGE = "strain_gauge"
    ACCELEROMETER = "accelerometer" 
    GYROSCOPE = "gyroscope"
    MAGNETIC_ENCODER = "magnetic_encoder"
    HALL_EFFECT = "hall_effect"
    PIEZOELECTRIC = "piezoelectric"
    THERMAL_FLOW = "thermal_flow"
    CORIOLIS_FLOW = "coriolis_flow"
    ELECTROMAGNETIC_FLOW = "electromagnetic_flow"
    INFRARED_THERMAL = "infrared_thermal"
    FORCE_SENSOR = "force_sensor"
    DISPLACEMENT_SENSOR = "displacement_sensor"
    ACOUSTIC = "acoustic"
    VIBRATION = "vibration"
    IMPEDANCE = "impedance"


class StrainGaugeSensor(ISensorInterface):
    """
    Strain gauge sensor to detect plunger movement
    
    Principle: Measures mechanical strain on syringe barrel or plunger
    Location: Attached to syringe barrel or plunger mechanism
    Advantages: High sensitivity, direct mechanical measurement
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_strain = 0.0
        self.gauge_factor = self.config.get('gauge_factor', 2.1)  # Typical for metal strain gauges
        self.bridge_voltage = self.config.get('bridge_voltage', 5.0)  # Bridge excitation voltage
        self.sensitivity_mv_per_v = self.config.get('sensitivity', 2.0)  # mV/V output
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.gauge_factor = config.get('gauge_factor', 2.1)
            self.bridge_voltage = config.get('bridge_voltage', 5.0)
            
            print(f"Strain gauge sensor initialized - Gauge factor: {self.gauge_factor}")
            return True
        except Exception as e:
            print(f"Strain gauge initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Calibrate with no force applied
            readings = []
            for _ in range(20):  # More samples for strain gauge
                readings.append(self._read_raw_strain())
                time.sleep(0.005)
            
            self.baseline_strain = sum(readings) / len(readings)
            self.is_calibrated_flag = True
            
            print(f"Strain gauge calibrated - Baseline: {self.baseline_strain:.6f} με")
            return True
        except Exception as e:
            print(f"Strain gauge calibration failed: {e}")
            return False
    
    def _read_raw_strain(self) -> float:
        """Read raw strain in microstrains (με)"""
        import random
        # Simulate strain reading during plunger movement
        # Typical values: 0-500 με for syringe operation
        base_strain = random.uniform(-10, 10)  # Baseline noise
        injection_strain = random.uniform(0, 300)  # Additional strain during injection
        return base_strain + injection_strain
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        raw_strain = self._read_raw_strain()
        calibrated_strain = raw_strain - self.baseline_strain if self.is_calibrated_flag else raw_strain
        
        # Convert strain to plunger displacement (requires calibration)
        displacement_mm = self._strain_to_displacement(calibrated_strain)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="strain_gauge",
            primary_value=calibrated_strain,
            secondary_value=displacement_mm,
            unit="με",  # microstrain
            quality=0.95,  # High quality for direct mechanical measurement
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _strain_to_displacement(self, strain_ue: float) -> float:
        """Convert strain to plunger displacement"""
        # This requires calibration based on syringe geometry
        # Typical: 100 με might correspond to 0.1mm displacement
        displacement_factor = self.config.get('displacement_factor', 0.001)  # mm per με
        return strain_ue * displacement_factor
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.LOAD_CELL  # Closest existing type
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_strain = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class AccelerometerSensor(ISensorInterface):
    """
    Accelerometer to detect syringe movement and injection dynamics
    
    Principle: Measures acceleration changes during injection
    Location: Attached to syringe body
    Advantages: Detects start/stop of injection, movement patterns
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_accel = {'x': 0.0, 'y': 0.0, 'z': 0.0}
        self.sensitivity_g = self.config.get('sensitivity_g', 2.0)  # ±2g range
        self.sampling_rate_hz = self.config.get('sampling_rate_hz', 100)
        self.accel_history = []
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.sensitivity_g = config.get('sensitivity_g', 2.0)
            self.sampling_rate_hz = config.get('sampling_rate_hz', 100)
            
            print(f"Accelerometer initialized - Range: ±{self.sensitivity_g}g, Rate: {self.sampling_rate_hz}Hz")
            return True
        except Exception as e:
            print(f"Accelerometer initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Calibrate for gravity offset
            readings_x, readings_y, readings_z = [], [], []
            
            for _ in range(50):
                accel = self._read_raw_acceleration()
                readings_x.append(accel['x'])
                readings_y.append(accel['y'])
                readings_z.append(accel['z'])
                time.sleep(0.01)
            
            self.baseline_accel = {
                'x': sum(readings_x) / len(readings_x),
                'y': sum(readings_y) / len(readings_y),
                'z': sum(readings_z) / len(readings_z)
            }
            self.is_calibrated_flag = True
            
            print(f"Accelerometer calibrated - Baseline: {self.baseline_accel}")
            return True
        except Exception as e:
            print(f"Accelerometer calibration failed: {e}")
            return False
    
    def _read_raw_acceleration(self) -> Dict[str, float]:
        """Read raw acceleration in g"""
        import random
        return {
            'x': random.uniform(-0.1, 0.1),  # Small movements
            'y': random.uniform(-0.1, 0.1),
            'z': random.uniform(0.9, 1.1)   # Gravity + small variations
        }
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        raw_accel = self._read_raw_acceleration()
        
        # Calculate calibrated acceleration magnitude
        if self.is_calibrated_flag:
            cal_x = raw_accel['x'] - self.baseline_accel['x']
            cal_y = raw_accel['y'] - self.baseline_accel['y']
            cal_z = raw_accel['z'] - self.baseline_accel['z']
        else:
            cal_x, cal_y, cal_z = raw_accel['x'], raw_accel['y'], raw_accel['z']
        
        # Calculate total acceleration magnitude
        magnitude = math.sqrt(cal_x**2 + cal_y**2 + cal_z**2)
        
        # Keep history for movement detection
        self.accel_history.append(magnitude)
        if len(self.accel_history) > 20:
            self.accel_history.pop(0)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="accelerometer",
            primary_value=magnitude,
            secondary_value=self._calculate_movement_variance(),
            unit="g",
            quality=0.8,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _calculate_movement_variance(self) -> float:
        """Calculate variance in acceleration to detect movement"""
        if len(self.accel_history) < 2:
            return 0.0
        
        import statistics
        return statistics.variance(self.accel_history)
    
    def detect_injection_movement(self, threshold_g: float = 0.05) -> bool:
        """Detect injection based on acceleration patterns"""
        if len(self.accel_history) < 10:
            return False
        
        recent_variance = self._calculate_movement_variance()
        return recent_variance > threshold_g
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.PROXIMITY  # Closest existing type
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_accel = {'x': 0.0, 'y': 0.0, 'z': 0.0}
        self.accel_history.clear()
        return True
    
    def self_test(self) -> bool:
        return True


class MagneticEncoderSensor(ISensorInterface):
    """
    Magnetic encoder to precisely track plunger position
    
    Principle: Tracks plunger movement using magnetic field changes
    Location: Magnet on plunger, sensor on syringe body
    Advantages: Very precise position tracking, no mechanical contact
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.initial_position = 0.0
        self.counts_per_mm = self.config.get('counts_per_mm', 100)  # Encoder resolution
        self.magnet_strength = self.config.get('magnet_strength', 1000)  # Gauss
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.counts_per_mm = config.get('counts_per_mm', 100)
            
            print(f"Magnetic encoder initialized - Resolution: {self.counts_per_mm} counts/mm")
            return True
        except Exception as e:
            print(f"Magnetic encoder initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Set initial plunger position
            self.initial_position = self._read_encoder_position()
            self.is_calibrated_flag = True
            
            print(f"Magnetic encoder calibrated - Initial position: {self.initial_position}")
            return True
        except Exception as e:
            print(f"Magnetic encoder calibration failed: {e}")
            return False
    
    def _read_encoder_position(self) -> float:
        """Read encoder position in mm"""
        import random
        # Simulate encoder position (0-50mm range for typical syringe)
        return random.uniform(0, 50)
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        current_position = self._read_encoder_position()
        
        # Calculate displacement from initial position
        displacement = current_position - self.initial_position if self.is_calibrated_flag else 0
        
        # Convert to volume based on syringe cross-sectional area
        volume_dispensed = self._position_to_volume(displacement)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="magnetic_encoder",
            primary_value=displacement,
            secondary_value=volume_dispensed,
            unit="mm",
            quality=0.98,  # Very high precision
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _position_to_volume(self, displacement_mm: float) -> float:
        """Convert plunger displacement to volume"""
        # Requires syringe cross-sectional area
        syringe_diameter_mm = self.config.get('syringe_diameter_mm', 14.5)  # 10mL syringe
        area_mm2 = math.pi * (syringe_diameter_mm / 2) ** 2
        volume_mm3 = displacement_mm * area_mm2
        return volume_mm3 / 1000.0  # Convert to mL
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.PROXIMITY
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.initial_position = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class ThermalFlowSensor(ISensorInterface):
    """
    Thermal flow sensor for direct flow measurement
    
    Principle: Measures heat transfer changes in flowing fluid
    Location: Inline with fluid path or around needle
    Advantages: Direct flow measurement, very sensitive
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_temperature = 0.0
        self.heater_power = self.config.get('heater_power_mw', 10)  # mW
        self.thermal_conductivity = self.config.get('thermal_conductivity', 0.6)  # W/m·K for water
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.heater_power = config.get('heater_power_mw', 10)
            
            print(f"Thermal flow sensor initialized - Heater power: {self.heater_power}mW")
            return True
        except Exception as e:
            print(f"Thermal flow sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Calibrate with no flow
            temps = []
            for _ in range(30):
                temps.append(self._read_temperature_differential())
                time.sleep(0.01)
            
            self.baseline_temperature = sum(temps) / len(temps)
            self.is_calibrated_flag = True
            
            print(f"Thermal flow sensor calibrated - Baseline ΔT: {self.baseline_temperature:.3f}°C")
            return True
        except Exception as e:
            print(f"Thermal flow sensor calibration failed: {e}")
            return False
    
    def _read_temperature_differential(self) -> float:
        """Read temperature differential across heater"""
        import random
        # Temperature difference decreases with flow rate
        base_temp_diff = 2.0  # °C with no flow
        flow_cooling = random.uniform(0, 1.5)  # Cooling due to flow
        return base_temp_diff - flow_cooling
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        temp_diff = self._read_temperature_differential()
        
        # Calculate flow rate from temperature differential
        flow_rate = self._temperature_to_flow_rate(temp_diff)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="thermal_flow",
            primary_value=flow_rate,
            secondary_value=temp_diff,
            unit="mL/s",
            quality=0.9,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _temperature_to_flow_rate(self, temp_diff: float) -> float:
        """Convert temperature differential to flow rate"""
        # Inverse relationship: higher flow = lower temp diff
        if temp_diff <= 0:
            return 0.0
        
        calibrated_temp = temp_diff - self.baseline_temperature if self.is_calibrated_flag else temp_diff
        
        # Empirical relationship (requires calibration)
        flow_factor = self.config.get('flow_factor', 0.5)  # mL/s per °C
        max_flow = 5.0  # mL/s maximum
        
        # Inverse relationship with saturation
        flow_rate = max(0.0, max_flow * (1.0 - calibrated_temp / 2.0))
        return flow_rate * flow_factor
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.FLOW_RATE
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_temperature = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


class AcousticSensor(ISensorInterface):
    """
    Acoustic sensor to detect injection sounds
    
    Principle: Microphone detects sound patterns during injection
    Location: On syringe body or nearby
    Advantages: Non-invasive, can detect air bubbles, flow patterns
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_noise = 0.0
        self.sampling_rate_hz = self.config.get('sampling_rate_hz', 1000)
        self.frequency_range = self.config.get('frequency_range_hz', (20, 20000))
        self.sound_history = []
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.sampling_rate_hz = config.get('sampling_rate_hz', 1000)
            
            print(f"Acoustic sensor initialized - Sampling: {self.sampling_rate_hz}Hz")
            return True
        except Exception as e:
            print(f"Acoustic sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Calibrate ambient noise level
            noise_samples = []
            for _ in range(100):
                noise_samples.append(self._read_sound_level())
                time.sleep(0.001)
            
            self.baseline_noise = sum(noise_samples) / len(noise_samples)
            self.is_calibrated_flag = True
            
            print(f"Acoustic sensor calibrated - Baseline: {self.baseline_noise:.1f} dB")
            return True
        except Exception as e:
            print(f"Acoustic sensor calibration failed: {e}")
            return False
    
    def _read_sound_level(self) -> float:
        """Read sound level in dB"""
        import random
        # Simulate sound level readings
        ambient_noise = random.uniform(30, 40)  # dB
        injection_sound = random.uniform(0, 20)  # Additional sound during injection
        return ambient_noise + injection_sound
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        sound_level = self._read_sound_level()
        
        # Calculate sound above baseline
        sound_above_baseline = sound_level - self.baseline_noise if self.is_calibrated_flag else 0
        
        # Keep history for pattern detection
        self.sound_history.append(sound_level)
        if len(self.sound_history) > 50:
            self.sound_history.pop(0)
        
        # Calculate sound pattern variance
        sound_variance = self._calculate_sound_variance()
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="acoustic",
            primary_value=sound_above_baseline,
            secondary_value=sound_variance,
            unit="dB",
            quality=0.7,  # Moderate quality due to noise sensitivity
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _calculate_sound_variance(self) -> float:
        """Calculate variance in sound levels"""
        if len(self.sound_history) < 2:
            return 0.0
        
        import statistics
        return statistics.variance(self.sound_history)
    
    def detect_injection_sound(self, threshold_db: float = 5.0) -> bool:
        """Detect injection based on sound pattern"""
        if not self.is_calibrated_flag or len(self.sound_history) < 10:
            return False
        
        recent_max = max(self.sound_history[-10:])
        return (recent_max - self.baseline_noise) > threshold_db
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.PROXIMITY
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_noise = 0.0
        self.sound_history.clear()
        return True
    
    def self_test(self) -> bool:
        return True


class ForceSensor(ISensorInterface):
    """
    Force sensor to measure injection force
    
    Principle: Measures force applied to plunger during injection
    Location: Behind plunger or in plunger mechanism
    Advantages: Direct measurement of injection effort, can detect blockages
    """
    
    def __init__(self, sensor_config: Optional[Dict[str, Any]] = None):
        self.config = sensor_config or {}
        self.is_calibrated_flag = False
        self.baseline_force = 0.0
        self.force_range_n = self.config.get('force_range_n', (0, 50))  # Newtons
        self.sensitivity = self.config.get('sensitivity_mv_per_n', 2.0)  # mV/N
        
    def initialize(self, config: Dict[str, Any]) -> bool:
        try:
            self.config.update(config)
            self.force_range_n = config.get('force_range_n', (0, 50))
            
            print(f"Force sensor initialized - Range: {self.force_range_n} N")
            return True
        except Exception as e:
            print(f"Force sensor initialization failed: {e}")
            return False
    
    def calibrate(self) -> bool:
        try:
            # Calibrate with no applied force
            force_readings = []
            for _ in range(20):
                force_readings.append(self._read_raw_force())
                time.sleep(0.01)
            
            self.baseline_force = sum(force_readings) / len(force_readings)
            self.is_calibrated_flag = True
            
            print(f"Force sensor calibrated - Baseline: {self.baseline_force:.3f} N")
            return True
        except Exception as e:
            print(f"Force sensor calibration failed: {e}")
            return False
    
    def _read_raw_force(self) -> float:
        """Read raw force in Newtons"""
        import random
        # Simulate force during injection (0-30N typical for manual injection)
        return random.uniform(0, 25)
    
    def get_reading(self, device_id: str, sensor_id: str) -> SensorReading:
        raw_force = self._read_raw_force()
        calibrated_force = raw_force - self.baseline_force if self.is_calibrated_flag else raw_force
        
        # Estimate flow rate from force (higher force = faster injection)
        estimated_flow_rate = self._force_to_flow_rate(calibrated_force)
        
        return SensorReading(
            timestamp=time.time(),
            sensor_type="force",
            primary_value=calibrated_force,
            secondary_value=estimated_flow_rate,
            unit="N",
            quality=0.9,
            temperature=25.0,
            device_id=device_id,
            sensor_id=sensor_id
        )
    
    def _force_to_flow_rate(self, force_n: float) -> float:
        """Estimate flow rate from applied force"""
        if force_n <= 0:
            return 0.0
        
        # Empirical relationship (requires calibration for specific syringe)
        # Typical: 10N might produce 1 mL/s flow rate
        flow_factor = self.config.get('force_to_flow_factor', 0.1)  # (mL/s)/N
        return force_n * flow_factor
    
    def detect_blockage(self, force_threshold_n: float = 20.0) -> bool:
        """Detect potential blockage based on high force"""
        current_force = self._read_raw_force()
        calibrated_force = current_force - self.baseline_force if self.is_calibrated_flag else current_force
        return calibrated_force > force_threshold_n
    
    def get_sensor_type(self) -> SensorType:
        return SensorType.LOAD_CELL
    
    def is_calibrated(self) -> bool:
        return self.is_calibrated_flag
    
    def reset(self) -> bool:
        self.is_calibrated_flag = False
        self.baseline_force = 0.0
        return True
    
    def self_test(self) -> bool:
        return True


# Sensor comparison and recommendations
def get_sensor_comparison() -> Dict[str, Dict[str, Any]]:
    """Get comprehensive comparison of all sensor options"""
    
    return {
        "Pressure Sensor (Tip)": {
            "invasiveness": "Minimal",
            "accuracy": "Excellent",
            "response_time": "Immediate",
            "cost": "Low-Medium",
            "complexity": "Low",
            "reliability": "High",
            "best_for": "Direct injection detection",
            "integration_difficulty": "Easy",
            "maintenance": "Low"
        },
        
        "Strain Gauge": {
            "invasiveness": "Low",
            "accuracy": "Excellent", 
            "response_time": "Immediate",
            "cost": "Low",
            "complexity": "Medium",
            "reliability": "High",
            "best_for": "Plunger movement tracking",
            "integration_difficulty": "Medium",
            "maintenance": "Low"
        },
        
        "Magnetic Encoder": {
            "invasiveness": "None",
            "accuracy": "Excellent",
            "response_time": "Immediate", 
            "cost": "Medium",
            "complexity": "Medium",
            "reliability": "High",
            "best_for": "Precise position tracking",
            "integration_difficulty": "Medium",
            "maintenance": "Low"
        },
        
        "Force Sensor": {
            "invasiveness": "Low",
            "accuracy": "Good",
            "response_time": "Immediate",
            "cost": "Medium", 
            "complexity": "Low",
            "reliability": "High",
            "best_for": "Injection effort monitoring",
            "integration_difficulty": "Easy",
            "maintenance": "Low"
        },
        
        "Thermal Flow": {
            "invasiveness": "Medium",
            "accuracy": "Excellent",
            "response_time": "Fast",
            "cost": "High",
            "complexity": "High",
            "reliability": "Medium",
            "best_for": "Direct flow measurement",
            "integration_difficulty": "Hard",
            "maintenance": "Medium"
        },
        
        "Accelerometer": {
            "invasiveness": "None",
            "accuracy": "Good",
            "response_time": "Fast",
            "cost": "Low",
            "complexity": "Medium",
            "reliability": "Medium",
            "best_for": "Movement pattern detection",
            "integration_difficulty": "Easy",
            "maintenance": "Low"
        },
        
        "Acoustic": {
            "invasiveness": "None",
            "accuracy": "Fair",
            "response_time": "Fast",
            "cost": "Low",
            "complexity": "High",
            "reliability": "Medium",
            "best_for": "Pattern recognition, air detection",
            "integration_difficulty": "Medium",
            "maintenance": "Low"
        },
        
        "Capacitive": {
            "invasiveness": "None",
            "accuracy": "Good",
            "response_time": "Fast",
            "cost": "Low",
            "complexity": "Low",
            "reliability": "Medium",
            "best_for": "Volume estimation",
            "integration_difficulty": "Easy",
            "maintenance": "Low"
        },
        
        "Optical": {
            "invasiveness": "None",
            "accuracy": "Good",
            "response_time": "Immediate",
            "cost": "Medium",
            "complexity": "Medium", 
            "reliability": "Medium",
            "best_for": "Presence detection, ICG monitoring",
            "integration_difficulty": "Medium",
            "maintenance": "Medium"
        }
    }


def get_recommended_sensor_combinations() -> Dict[str, List[str]]:
    """Get recommended sensor combinations for different use cases"""
    
    return {
        "Maximum Accuracy": [
            "Pressure Sensor (Tip)",
            "Magnetic Encoder", 
            "Force Sensor",
            "Thermal Flow"
        ],
        
        "Cost Effective": [
            "Strain Gauge",
            "Accelerometer",
            "Capacitive"
        ],
        
        "Non-Invasive": [
            "Magnetic Encoder",
            "Accelerometer", 
            "Acoustic",
            "Optical"
        ],
        
        "High Reliability": [
            "Pressure Sensor (Tip)",
            "Strain Gauge",
            "Force Sensor",
            "Magnetic Encoder"
        ],
        
        "Research/Development": [
            "All sensors for comparison and optimization"
        ],
        
        "Production Ready": [
            "Pressure Sensor (Tip)",
            "Strain Gauge",
            "Accelerometer"
        ]
    }


if __name__ == "__main__":
    print("🔬 Advanced Sensor Options for Smart Sleeve System")
    print("=" * 60)
    
    print("\n📊 SENSOR COMPARISON:")
    comparison = get_sensor_comparison()
    
    for sensor_name, properties in comparison.items():
        print(f"\n{sensor_name}:")
        print(f"  Accuracy: {properties['accuracy']}")
        print(f"  Cost: {properties['cost']}")
        print(f"  Invasiveness: {properties['invasiveness']}")
        print(f"  Best for: {properties['best_for']}")
    
    print("\n🎯 RECOMMENDED COMBINATIONS:")
    combinations = get_recommended_sensor_combinations()
    
    for use_case, sensors in combinations.items():
        print(f"\n{use_case}:")
        for sensor in sensors:
            print(f"  • {sensor}")