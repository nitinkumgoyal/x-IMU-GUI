#!/usr/bin/env python3
"""
Smart Sleeve System - Example Usage

This example demonstrates how to use the generic smart sleeve system with
different communication protocols (WiFi, BLE, UWB, etc.)
"""

import time
import asyncio
from typing import Optional

from smart_sleeve_system import (
    SmartSleeveController, CommunicationFactory, CommunicationProtocol,
    SensorReading, VolumeData
)
from config_system import ConfigurationManager
from communication_interfaces import *


class SmartSleeveApplication:
    """Example application using the smart sleeve system"""
    
    def __init__(self, config_file: str = "smart_sleeve_config.json"):
        self.config_manager = ConfigurationManager(config_file)
        self.controller: Optional[SmartSleeveController] = None
        self.running = False
        
    def setup_system(self, protocol: CommunicationProtocol, device_id: str = "sleeve_demo_001"):
        """Setup the smart sleeve system with specified protocol"""
        
        print(f"Setting up Smart Sleeve with {protocol.value} communication...")
        
        # Create or load configuration
        config = self.config_manager.load_config()
        if not config:
            print("Creating default configuration...")
            config = self.config_manager.create_default_config(protocol, device_id)
            self.config_manager.save_config(config)
        
        # Validate configuration
        is_valid, errors = self.config_manager.validate_config(config)
        if not is_valid:
            print("Configuration validation failed:")
            for error in errors:
                print(f"  - {error}")
            return False
        
        # Create communication interface
        comm_interface = CommunicationFactory.create_interface(protocol)
        
        # Create smart sleeve controller
        self.controller = SmartSleeveController(config.device_id, comm_interface)
        
        # Register event callbacks
        self.controller.register_callback("filling_started", self._on_filling_started)
        self.controller.register_callback("injection_detected", self._on_injection_detected)
        
        # Initialize system
        success = self.controller.initialize_system(
            config.communication_config.protocol_config,
            config.communication_config.hub_address
        )
        
        if success:
            print(f"✓ Smart Sleeve {device_id} initialized successfully")
            print(f"✓ Communication: {protocol.value}")
            print(f"✓ Hub address: {config.communication_config.hub_address}")
        else:
            print("✗ Failed to initialize smart sleeve system")
        
        return success
    
    def _on_filling_started(self, initial_reading: SensorReading):
        """Callback for when syringe filling starts"""
        print(f"\n📋 Syringe filling started at {time.strftime('%H:%M:%S')}")
        print(f"   Initial capacitance: {initial_reading.capacitance_value:.2f}")
        print(f"   Temperature: {initial_reading.temperature:.1f}°C")
        print(f"   Battery: {initial_reading.battery_level:.1f}%")
    
    def _on_injection_detected(self, volume_data: VolumeData, sensor_reading: SensorReading):
        """Callback for when injection is detected"""
        print(f"\n💉 INJECTION DETECTED at {time.strftime('%H:%M:%S')}")
        print(f"   Injected volume: {volume_data.injected_volume:.2f} mL")
        print(f"   Remaining volume: {volume_data.current_volume:.2f} mL")
        print(f"   Confidence: {volume_data.confidence_level:.1%}")
        print(f"   🔄 Triggering quantification algorithm on Connected OR HUB")
    
    async def run_monitoring_loop(self, duration_seconds: int = 60):
        """Run the main monitoring loop"""
        if not self.controller:
            print("Controller not initialized")
            return
        
        print(f"\n🔍 Starting monitoring loop for {duration_seconds} seconds...")
        
        # Start filling monitoring
        if not self.controller.start_filling_monitoring():
            print("Failed to start filling monitoring")
            return
        
        self.running = True
        start_time = time.time()
        
        try:
            while self.running and (time.time() - start_time) < duration_seconds:
                # Monitor for injection
                volume_data = self.controller.monitor_injection()
                
                if volume_data:
                    # Print periodic status (every 5 seconds)
                    if int(time.time()) % 5 == 0:
                        print(f"⏱️  Status - Current: {volume_data.current_volume:.2f}mL, "
                              f"Injected: {volume_data.injected_volume:.2f}mL")
                
                # Check system status
                status = self.controller.get_system_status()
                if not status['is_connected']:
                    print("⚠️  Warning: Lost connection to hub")
                
                if status['battery_level'] < 20:
                    print(f"🔋 Warning: Low battery {status['battery_level']:.1f}%")
                
                await asyncio.sleep(0.1)  # 100ms monitoring interval
                
        except KeyboardInterrupt:
            print("\n⏹️  Monitoring stopped by user")
        
        finally:
            self.running = False
            print("🔄 Shutting down system...")
            self.controller.shutdown()
    
    def stop_monitoring(self):
        """Stop the monitoring loop"""
        self.running = False
    
    def switch_protocol(self, new_protocol: CommunicationProtocol, hub_address: str = None):
        """Switch to a different communication protocol"""
        if not self.config_manager.current_config:
            print("No configuration loaded")
            return False
        
        print(f"🔄 Switching to {new_protocol.value} protocol...")
        
        success = self.config_manager.switch_communication_protocol(new_protocol, hub_address)
        
        if success:
            print(f"✓ Successfully switched to {new_protocol.value}")
            # Reinitialize system with new protocol
            return self.setup_system(new_protocol, self.config_manager.current_config.device_id)
        else:
            print(f"✗ Failed to switch to {new_protocol.value}")
            return False
    
    def get_system_info(self):
        """Display current system information"""
        if not self.controller:
            print("System not initialized")
            return
        
        status = self.controller.get_system_status()
        config = self.config_manager.get_current_config()
        
        print("\n📊 SYSTEM STATUS")
        print("=" * 50)
        print(f"Device ID: {status['device_id']}")
        print(f"Protocol: {config.communication_config.protocol}")
        print(f"Hub Address: {config.communication_config.hub_address}")
        print(f"Connected: {'✓' if status['is_connected'] else '✗'}")
        print(f"Monitoring: {'✓' if status['is_monitoring'] else '✗'}")
        print(f"Sensor Calibrated: {'✓' if status['sensor_calibrated'] else '✗'}")
        print(f"Signal Strength: {status['signal_strength']:.1f}%")
        print(f"Battery Level: {status['battery_level']:.1f}%")
        print(f"Temperature: {status['temperature']:.1f}°C")
        print("=" * 50)


async def demo_bluetooth_le():
    """Demonstrate Bluetooth LE communication"""
    print("\n🔵 BLUETOOTH LE DEMO")
    print("=" * 50)
    
    app = SmartSleeveApplication("ble_config.json")
    
    if app.setup_system(CommunicationProtocol.BLUETOOTH_LE, "sleeve_ble_001"):
        app.get_system_info()
        await app.run_monitoring_loop(30)  # Run for 30 seconds


async def demo_wifi():
    """Demonstrate WiFi communication"""
    print("\n📶 WiFi DEMO")
    print("=" * 50)
    
    app = SmartSleeveApplication("wifi_config.json")
    
    if app.setup_system(CommunicationProtocol.WIFI, "sleeve_wifi_001"):
        app.get_system_info()
        await app.run_monitoring_loop(30)  # Run for 30 seconds


async def demo_uwb():
    """Demonstrate UWB communication"""
    print("\n📡 UWB DEMO")
    print("=" * 50)
    
    app = SmartSleeveApplication("uwb_config.json")
    
    if app.setup_system(CommunicationProtocol.UWB, "sleeve_uwb_001"):
        app.get_system_info()
        await app.run_monitoring_loop(30)  # Run for 30 seconds


async def demo_protocol_switching():
    """Demonstrate switching between different protocols"""
    print("\n🔄 PROTOCOL SWITCHING DEMO")
    print("=" * 50)
    
    app = SmartSleeveApplication("multi_protocol_config.json")
    
    # Start with WiFi
    print("\n1️⃣ Starting with WiFi...")
    if app.setup_system(CommunicationProtocol.WIFI, "sleeve_multi_001"):
        app.get_system_info()
        await asyncio.sleep(5)  # Run briefly
        
        # Switch to Bluetooth LE
        print("\n2️⃣ Switching to Bluetooth LE...")
        if app.switch_protocol(CommunicationProtocol.BLUETOOTH_LE):
            app.get_system_info()
            await asyncio.sleep(5)  # Run briefly
            
            # Switch to UWB
            print("\n3️⃣ Switching to UWB...")
            if app.switch_protocol(CommunicationProtocol.UWB):
                app.get_system_info()
                await asyncio.sleep(5)  # Run briefly


def create_config_templates():
    """Create configuration templates for all supported protocols"""
    print("📝 Creating configuration templates...")
    
    config_manager = ConfigurationManager()
    
    protocols = [
        CommunicationProtocol.BLUETOOTH_LE,
        CommunicationProtocol.WIFI,
        CommunicationProtocol.UWB,
        CommunicationProtocol.ZIGBEE,
        CommunicationProtocol.LORA,
        CommunicationProtocol.CELLULAR
    ]
    
    for protocol in protocols:
        filename = f"template_{protocol.value}_config.json"
        if config_manager.export_config_template(protocol, filename):
            print(f"✓ Created {filename}")
        else:
            print(f"✗ Failed to create {filename}")


async def main():
    """Main demonstration function"""
    print("🩺 Smart Sleeve System - Generic Communication Demo")
    print("=" * 60)
    
    # Create configuration templates
    create_config_templates()
    
    # Run individual protocol demos
    await demo_wifi()
    await demo_bluetooth_le()
    await demo_uwb()
    
    # Demonstrate protocol switching
    await demo_protocol_switching()
    
    print("\n✅ Demo completed successfully!")
    print("\nSupported protocols:")
    config_manager = ConfigurationManager()
    for protocol in config_manager.get_supported_protocols():
        print(f"  - {protocol}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Demo interrupted by user")
    except Exception as e:
        print(f"\n❌ Demo failed: {e}")
        import traceback
        traceback.print_exc()