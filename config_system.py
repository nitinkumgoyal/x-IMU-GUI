import json
import os
from typing import Dict, Any, Optional
from dataclasses import dataclass, asdict
from smart_sleeve_system import CommunicationProtocol


@dataclass
class SensorConfig:
    """Configuration for capacitive sensor"""
    calibration_factor: float = 0.1
    temperature_compensation: bool = True
    noise_filter_enabled: bool = True
    sampling_rate_hz: int = 100
    baseline_samples: int = 10


@dataclass
class CommunicationConfig:
    """Base configuration for communication protocols"""
    protocol: str
    hub_address: str
    retry_attempts: int = 3
    timeout_seconds: int = 30
    
    # Protocol-specific configurations
    protocol_config: Dict[str, Any] = None
    
    def __post_init__(self):
        if self.protocol_config is None:
            self.protocol_config = {}


@dataclass 
class BLEConfig(CommunicationConfig):
    """Bluetooth LE specific configuration"""
    service_uuid: str = "12345678-1234-1234-1234-123456789abc"
    characteristic_uuid: str = "87654321-4321-4321-4321-cba987654321"
    connection_interval_ms: int = 100
    mtu_size: int = 247
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.BLUETOOTH_LE.value
        self.protocol_config = {
            'service_uuid': self.service_uuid,
            'characteristic_uuid': self.characteristic_uuid,
            'connection_interval_ms': self.connection_interval_ms,
            'mtu_size': self.mtu_size
        }


@dataclass
class WiFiConfig(CommunicationConfig):
    """WiFi specific configuration"""
    port: int = 8080
    use_tcp: bool = True
    ssid: str = ""
    password: str = ""
    encryption: str = "WPA2"
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.WIFI.value
        self.protocol_config = {
            'port': self.port,
            'use_tcp': self.use_tcp,
            'ssid': self.ssid,
            'password': self.password,
            'encryption': self.encryption
        }


@dataclass
class UWBConfig(CommunicationConfig):
    """UWB specific configuration"""
    ranging_enabled: bool = True
    channel: int = 5
    prf: int = 64  # Pulse Repetition Frequency (16 or 64 MHz)
    data_rate: str = "6.8M"  # 110K, 850K, 6.8M
    preamble_length: int = 1024
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.UWB.value
        self.protocol_config = {
            'ranging_enabled': self.ranging_enabled,
            'channel': self.channel,
            'prf': self.prf,
            'data_rate': self.data_rate,
            'preamble_length': self.preamble_length
        }


@dataclass
class ZigBeeConfig(CommunicationConfig):
    """ZigBee specific configuration"""
    pan_id: int = 0x1234
    channel: int = 11
    network_key: str = ""
    device_type: str = "end_device"  # coordinator, router, end_device
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.ZIGBEE.value
        self.protocol_config = {
            'pan_id': self.pan_id,
            'channel': self.channel,
            'network_key': self.network_key,
            'device_type': self.device_type
        }


@dataclass
class LoRaConfig(CommunicationConfig):
    """LoRa/LoRaWAN specific configuration"""
    spreading_factor: int = 7  # 7-12
    bandwidth: int = 125000  # 125000, 250000, 500000 Hz
    coding_rate: str = "4/5"  # 4/5, 4/6, 4/7, 4/8
    frequency: int = 868100000  # Hz
    sync_word: int = 0x12
    
    # LoRaWAN specific
    use_lorawan: bool = True
    device_eui: str = ""
    application_eui: str = ""
    application_key: str = ""
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.LORA.value
        self.protocol_config = {
            'spreading_factor': self.spreading_factor,
            'bandwidth': self.bandwidth,
            'coding_rate': self.coding_rate,
            'frequency': self.frequency,
            'sync_word': self.sync_word,
            'use_lorawan': self.use_lorawan,
            'device_eui': self.device_eui,
            'application_eui': self.application_eui,
            'application_key': self.application_key
        }


@dataclass
class CellularConfig(CommunicationConfig):
    """Cellular specific configuration"""
    apn: str = "internet"
    network_type: str = "4G"  # 2G, 3G, 4G, 5G
    username: str = ""
    password: str = ""
    pin: str = ""
    
    # Communication method
    use_mqtt: bool = True
    mqtt_broker: str = ""
    mqtt_port: int = 1883
    mqtt_topic: str = "smart_sleeve"
    
    def __post_init__(self):
        super().__post_init__()
        self.protocol = CommunicationProtocol.CELLULAR.value
        self.protocol_config = {
            'apn': self.apn,
            'network_type': self.network_type,
            'username': self.username,
            'password': self.password,
            'pin': self.pin,
            'use_mqtt': self.use_mqtt,
            'mqtt_broker': self.mqtt_broker,
            'mqtt_port': self.mqtt_port,
            'mqtt_topic': self.mqtt_topic
        }


@dataclass
class SmartSleeveConfig:
    """Complete smart sleeve system configuration"""
    device_id: str
    device_name: str
    firmware_version: str
    
    # Sensor configuration
    sensor_config: SensorConfig
    
    # Communication configuration
    communication_config: CommunicationConfig
    
    # System settings
    injection_threshold_ml: float = 0.1
    monitoring_interval_ms: int = 100
    battery_warning_threshold: float = 20.0
    temperature_warning_threshold: float = 35.0
    
    # Data logging
    enable_local_logging: bool = True
    log_level: str = "INFO"  # DEBUG, INFO, WARNING, ERROR
    max_log_size_mb: int = 10


class ConfigurationManager:
    """Manages configuration loading, saving, and validation"""
    
    def __init__(self, config_file: str = "smart_sleeve_config.json"):
        self.config_file = config_file
        self.current_config: Optional[SmartSleeveConfig] = None
    
    def create_default_config(self, protocol: CommunicationProtocol, device_id: str = "sleeve_001") -> SmartSleeveConfig:
        """Create default configuration for specified protocol"""
        
        sensor_config = SensorConfig()
        
        # Create protocol-specific communication config
        if protocol == CommunicationProtocol.BLUETOOTH_LE:
            comm_config = BLEConfig(hub_address="hub_ble_address")
        elif protocol == CommunicationProtocol.WIFI:
            comm_config = WiFiConfig(hub_address="192.168.1.100")
        elif protocol == CommunicationProtocol.UWB:
            comm_config = UWBConfig(hub_address="uwb_anchor_01")
        elif protocol == CommunicationProtocol.ZIGBEE:
            comm_config = ZigBeeConfig(hub_address="zigbee_coordinator")
        elif protocol == CommunicationProtocol.LORA:
            comm_config = LoRaConfig(hub_address="lora_gateway_01")
        elif protocol == CommunicationProtocol.CELLULAR:
            comm_config = CellularConfig(hub_address="smart-sleeve-hub.com")
        else:
            raise ValueError(f"Unsupported protocol: {protocol}")
        
        return SmartSleeveConfig(
            device_id=device_id,
            device_name=f"Smart Sleeve {device_id}",
            firmware_version="1.0.0",
            sensor_config=sensor_config,
            communication_config=comm_config
        )
    
    def load_config(self) -> Optional[SmartSleeveConfig]:
        """Load configuration from file"""
        try:
            if not os.path.exists(self.config_file):
                return None
            
            with open(self.config_file, 'r') as f:
                config_dict = json.load(f)
            
            # Reconstruct the configuration objects
            sensor_config = SensorConfig(**config_dict['sensor_config'])
            
            # Determine communication protocol and create appropriate config
            comm_protocol = config_dict['communication_config']['protocol']
            comm_data = config_dict['communication_config']
            
            if comm_protocol == CommunicationProtocol.BLUETOOTH_LE.value:
                comm_config = BLEConfig(**comm_data)
            elif comm_protocol == CommunicationProtocol.WIFI.value:
                comm_config = WiFiConfig(**comm_data)
            elif comm_protocol == CommunicationProtocol.UWB.value:
                comm_config = UWBConfig(**comm_data)
            elif comm_protocol == CommunicationProtocol.ZIGBEE.value:
                comm_config = ZigBeeConfig(**comm_data)
            elif comm_protocol == CommunicationProtocol.LORA.value:
                comm_config = LoRaConfig(**comm_data)
            elif comm_protocol == CommunicationProtocol.CELLULAR.value:
                comm_config = CellularConfig(**comm_data)
            else:
                raise ValueError(f"Unknown protocol: {comm_protocol}")
            
            # Create main config object
            config_data = {k: v for k, v in config_dict.items() 
                          if k not in ['sensor_config', 'communication_config']}
            config_data['sensor_config'] = sensor_config
            config_data['communication_config'] = comm_config
            
            self.current_config = SmartSleeveConfig(**config_data)
            return self.current_config
            
        except Exception as e:
            print(f"Failed to load configuration: {e}")
            return None
    
    def save_config(self, config: SmartSleeveConfig) -> bool:
        """Save configuration to file"""
        try:
            config_dict = asdict(config)
            
            with open(self.config_file, 'w') as f:
                json.dump(config_dict, f, indent=2)
            
            self.current_config = config
            return True
            
        except Exception as e:
            print(f"Failed to save configuration: {e}")
            return False
    
    def validate_config(self, config: SmartSleeveConfig) -> tuple[bool, list[str]]:
        """Validate configuration and return errors if any"""
        errors = []
        
        # Validate device info
        if not config.device_id or len(config.device_id) < 3:
            errors.append("Device ID must be at least 3 characters")
        
        # Validate sensor config
        if config.sensor_config.calibration_factor <= 0:
            errors.append("Calibration factor must be positive")
        
        if config.sensor_config.sampling_rate_hz < 1 or config.sensor_config.sampling_rate_hz > 1000:
            errors.append("Sampling rate must be between 1-1000 Hz")
        
        # Validate communication config
        if not config.communication_config.hub_address:
            errors.append("Hub address is required")
        
        # Protocol-specific validation
        protocol = config.communication_config.protocol
        if protocol == CommunicationProtocol.WIFI.value:
            wifi_config = config.communication_config
            if wifi_config.port < 1 or wifi_config.port > 65535:
                errors.append("WiFi port must be between 1-65535")
        
        elif protocol == CommunicationProtocol.LORA.value:
            lora_config = config.communication_config
            if lora_config.spreading_factor < 7 or lora_config.spreading_factor > 12:
                errors.append("LoRa spreading factor must be between 7-12")
        
        # Validate system settings
        if config.injection_threshold_ml < 0:
            errors.append("Injection threshold must be non-negative")
        
        if config.monitoring_interval_ms < 10:
            errors.append("Monitoring interval must be at least 10ms")
        
        return len(errors) == 0, errors
    
    def get_current_config(self) -> Optional[SmartSleeveConfig]:
        """Get currently loaded configuration"""
        return self.current_config
    
    def switch_communication_protocol(self, new_protocol: CommunicationProtocol, hub_address: str = None) -> bool:
        """Switch to a different communication protocol"""
        if not self.current_config:
            return False
        
        try:
            # Create new communication config for the protocol
            if new_protocol == CommunicationProtocol.BLUETOOTH_LE:
                new_comm_config = BLEConfig(hub_address=hub_address or "hub_ble_address")
            elif new_protocol == CommunicationProtocol.WIFI:
                new_comm_config = WiFiConfig(hub_address=hub_address or "192.168.1.100")
            elif new_protocol == CommunicationProtocol.UWB:
                new_comm_config = UWBConfig(hub_address=hub_address or "uwb_anchor_01")
            elif new_protocol == CommunicationProtocol.ZIGBEE:
                new_comm_config = ZigBeeConfig(hub_address=hub_address or "zigbee_coordinator")
            elif new_protocol == CommunicationProtocol.LORA:
                new_comm_config = LoRaConfig(hub_address=hub_address or "lora_gateway_01")
            elif new_protocol == CommunicationProtocol.CELLULAR:
                new_comm_config = CellularConfig(hub_address=hub_address or "smart-sleeve-hub.com")
            else:
                return False
            
            # Update current config
            self.current_config.communication_config = new_comm_config
            
            # Save updated config
            return self.save_config(self.current_config)
            
        except Exception as e:
            print(f"Failed to switch protocol: {e}")
            return False
    
    def get_supported_protocols(self) -> list[str]:
        """Get list of supported communication protocols"""
        return [protocol.value for protocol in CommunicationProtocol]
    
    def export_config_template(self, protocol: CommunicationProtocol, filename: str) -> bool:
        """Export a configuration template for the specified protocol"""
        try:
            template_config = self.create_default_config(protocol, "template_device")
            template_dict = asdict(template_config)
            
            with open(filename, 'w') as f:
                json.dump(template_dict, f, indent=2)
            
            return True
            
        except Exception as e:
            print(f"Failed to export template: {e}")
            return False