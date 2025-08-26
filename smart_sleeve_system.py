from abc import ABC, abstractmethod
from enum import Enum
from typing import Dict, Any, Optional, Callable
import time
import json
from dataclasses import dataclass, asdict


class CommunicationProtocol(Enum):
    """Supported communication protocols"""
    BLUETOOTH_LE = "ble"
    WIFI = "wifi"
    UWB = "uwb"
    ZIGBEE = "zigbee"
    LORA = "lora"
    CELLULAR = "cellular"


@dataclass
class SensorReading:
    """Data structure for capacitive sensor readings"""
    timestamp: float
    capacitance_value: float
    temperature: float
    battery_level: float
    device_id: str


@dataclass
class VolumeData:
    """Data structure for volume calculations"""
    initial_volume: float
    current_volume: float
    injected_volume: float
    timestamp: float
    confidence_level: float


@dataclass
class SmartSleeveMessage:
    """Generic message format for communication with Connected OR HUB"""
    message_type: str
    device_id: str
    timestamp: float
    data: Dict[str, Any]
    protocol_used: str


class ICommunicationInterface(ABC):
    """Abstract interface for communication protocols"""
    
    @abstractmethod
    def initialize(self, config: Dict[str, Any]) -> bool:
        """Initialize the communication interface with specific configuration"""
        pass
    
    @abstractmethod
    def connect(self, hub_address: str) -> bool:
        """Connect to the Connected OR HUB"""
        pass
    
    @abstractmethod
    def send_message(self, message: SmartSleeveMessage) -> bool:
        """Send a message to the Connected OR HUB"""
        pass
    
    @abstractmethod
    def receive_message(self) -> Optional[SmartSleeveMessage]:
        """Receive a message from the Connected OR HUB"""
        pass
    
    @abstractmethod
    def disconnect(self) -> bool:
        """Disconnect from the Connected OR HUB"""
        pass
    
    @abstractmethod
    def is_connected(self) -> bool:
        """Check if currently connected"""
        pass
    
    @abstractmethod
    def get_signal_strength(self) -> float:
        """Get current signal strength/quality"""
        pass


class CapacitiveSensor:
    """Capacitive sensor interface for liquid detection and measurement"""
    
    def __init__(self, calibration_data: Optional[Dict[str, float]] = None):
        self.calibration_data = calibration_data or {}
        self.baseline_reading = 0.0
        self.is_calibrated = False
        
    def calibrate_empty_syringe(self) -> bool:
        """Calibrate sensor with empty syringe"""
        try:
            # Simulate calibration - in real implementation, this would read from actual sensor
            self.baseline_reading = self._read_raw_capacitance()
            self.is_calibrated = True
            return True
        except Exception:
            return False
    
    def _read_raw_capacitance(self) -> float:
        """Read raw capacitance value from sensor hardware"""
        # Placeholder for actual sensor reading implementation
        # In real implementation, this would interface with ADC/sensor chip
        import random
        return random.uniform(100.0, 500.0)  # Simulated reading
    
    def get_sensor_reading(self, device_id: str) -> SensorReading:
        """Get current sensor reading with metadata"""
        raw_value = self._read_raw_capacitance()
        
        return SensorReading(
            timestamp=time.time(),
            capacitance_value=raw_value,
            temperature=self._get_temperature(),
            battery_level=self._get_battery_level(),
            device_id=device_id
        )
    
    def _get_temperature(self) -> float:
        """Get current temperature for compensation"""
        # Placeholder for temperature sensor
        return 23.5
    
    def _get_battery_level(self) -> float:
        """Get current battery level percentage"""
        # Placeholder for battery monitoring
        return 85.0
    
    def calculate_volume(self, current_reading: float, initial_reading: float) -> float:
        """Calculate liquid volume based on capacitance readings"""
        if not self.is_calibrated:
            raise ValueError("Sensor not calibrated")
        
        # Volume calculation based on capacitance change
        # This would be calibrated based on syringe geometry and liquid properties
        capacitance_diff = current_reading - initial_reading
        
        # Convert capacitance change to volume (mL)
        # Calibration factor would be determined experimentally
        volume_ml = capacitance_diff * self.calibration_data.get('volume_factor', 0.1)
        return max(0.0, volume_ml)


class SmartSleeveController:
    """Main controller for the smart sleeve system"""
    
    def __init__(self, device_id: str, communication_interface: ICommunicationInterface):
        self.device_id = device_id
        self.comm_interface = communication_interface
        self.sensor = CapacitiveSensor()
        self.initial_reading: Optional[SensorReading] = None
        self.is_monitoring = False
        self.injection_threshold = 0.1  # mL threshold for injection detection
        self.callbacks: Dict[str, Callable] = {}
        
    def register_callback(self, event_type: str, callback: Callable):
        """Register callbacks for events"""
        self.callbacks[event_type] = callback
    
    def initialize_system(self, comm_config: Dict[str, Any], hub_address: str) -> bool:
        """Initialize the complete smart sleeve system"""
        try:
            # Initialize communication
            if not self.comm_interface.initialize(comm_config):
                return False
            
            # Connect to hub
            if not self.comm_interface.connect(hub_address):
                return False
            
            # Calibrate sensor
            if not self.sensor.calibrate_empty_syringe():
                return False
            
            # Send initialization message
            init_message = SmartSleeveMessage(
                message_type="system_init",
                device_id=self.device_id,
                timestamp=time.time(),
                data={"status": "initialized", "sensor_calibrated": True},
                protocol_used=comm_config.get('protocol', 'unknown')
            )
            
            return self.comm_interface.send_message(init_message)
            
        except Exception as e:
            print(f"System initialization failed: {e}")
            return False
    
    def start_filling_monitoring(self) -> bool:
        """Start monitoring syringe filling"""
        try:
            self.initial_reading = self.sensor.get_sensor_reading(self.device_id)
            self.is_monitoring = True
            
            # Send filling start message
            message = SmartSleeveMessage(
                message_type="filling_started",
                device_id=self.device_id,
                timestamp=time.time(),
                data=asdict(self.initial_reading),
                protocol_used=self.comm_interface.__class__.__name__
            )
            
            self.comm_interface.send_message(message)
            
            if "filling_started" in self.callbacks:
                self.callbacks["filling_started"](self.initial_reading)
            
            return True
            
        except Exception as e:
            print(f"Failed to start filling monitoring: {e}")
            return False
    
    def monitor_injection(self) -> Optional[VolumeData]:
        """Monitor for injection events and calculate volume"""
        if not self.is_monitoring or not self.initial_reading:
            return None
        
        try:
            current_reading = self.sensor.get_sensor_reading(self.device_id)
            
            # Calculate volumes
            current_volume = self.sensor.calculate_volume(
                current_reading.capacitance_value, 
                self.sensor.baseline_reading
            )
            initial_volume = self.sensor.calculate_volume(
                self.initial_reading.capacitance_value, 
                self.sensor.baseline_reading
            )
            
            injected_volume = initial_volume - current_volume
            
            volume_data = VolumeData(
                initial_volume=initial_volume,
                current_volume=current_volume,
                injected_volume=injected_volume,
                timestamp=time.time(),
                confidence_level=self._calculate_confidence(current_reading)
            )
            
            # Check if injection threshold is met
            if injected_volume >= self.injection_threshold:
                self._trigger_injection_event(volume_data, current_reading)
            
            return volume_data
            
        except Exception as e:
            print(f"Injection monitoring error: {e}")
            return None
    
    def _calculate_confidence(self, reading: SensorReading) -> float:
        """Calculate confidence level of the measurement"""
        # Factors affecting confidence: temperature stability, battery level, signal quality
        temp_factor = 1.0 if 20 <= reading.temperature <= 30 else 0.8
        battery_factor = reading.battery_level / 100.0
        signal_factor = min(1.0, self.comm_interface.get_signal_strength() / 100.0)
        
        return min(1.0, temp_factor * battery_factor * signal_factor)
    
    def _trigger_injection_event(self, volume_data: VolumeData, sensor_reading: SensorReading):
        """Trigger injection detection event"""
        # Send injection trigger message to Connected OR HUB
        message = SmartSleeveMessage(
            message_type="injection_detected",
            device_id=self.device_id,
            timestamp=time.time(),
            data={
                "volume_data": asdict(volume_data),
                "sensor_reading": asdict(sensor_reading),
                "trigger_quantification": True
            },
            protocol_used=self.comm_interface.__class__.__name__
        )
        
        self.comm_interface.send_message(message)
        
        if "injection_detected" in self.callbacks:
            self.callbacks["injection_detected"](volume_data, sensor_reading)
    
    def get_system_status(self) -> Dict[str, Any]:
        """Get current system status"""
        return {
            "device_id": self.device_id,
            "is_monitoring": self.is_monitoring,
            "is_connected": self.comm_interface.is_connected(),
            "sensor_calibrated": self.sensor.is_calibrated,
            "signal_strength": self.comm_interface.get_signal_strength(),
            "battery_level": self.sensor._get_battery_level(),
            "temperature": self.sensor._get_temperature()
        }
    
    def shutdown(self):
        """Gracefully shutdown the system"""
        self.is_monitoring = False
        
        # Send shutdown message
        message = SmartSleeveMessage(
            message_type="system_shutdown",
            device_id=self.device_id,
            timestamp=time.time(),
            data={"reason": "normal_shutdown"},
            protocol_used=self.comm_interface.__class__.__name__
        )
        
        self.comm_interface.send_message(message)
        self.comm_interface.disconnect()


class CommunicationFactory:
    """Factory for creating communication interface instances"""
    
    @staticmethod
    def create_interface(protocol: CommunicationProtocol) -> ICommunicationInterface:
        """Create appropriate communication interface based on protocol"""
        # Import here to avoid circular imports
        from communication_interfaces import (
            BluetoothLEInterface, WiFiInterface, UWBInterface, 
            ZigBeeInterface, LoRaInterface, CellularInterface
        )
        
        if protocol == CommunicationProtocol.BLUETOOTH_LE:
            return BluetoothLEInterface()
        elif protocol == CommunicationProtocol.WIFI:
            return WiFiInterface()
        elif protocol == CommunicationProtocol.UWB:
            return UWBInterface()
        elif protocol == CommunicationProtocol.ZIGBEE:
            return ZigBeeInterface()
        elif protocol == CommunicationProtocol.LORA:
            return LoRaInterface()
        elif protocol == CommunicationProtocol.CELLULAR:
            return CellularInterface()
        else:
            raise ValueError(f"Unsupported communication protocol: {protocol}")