#!/usr/bin/env python3
"""
Enhanced Smart Sleeve System - Multi-Sensor Example

This example demonstrates the enhanced smart sleeve system using multiple
sensor types for improved accuracy and reliability in fluid dispensing detection.
"""

import asyncio
import time
from typing import Dict, Any

from smart_sleeve_system import CommunicationFactory, CommunicationProtocol
from enhanced_smart_sleeve import EnhancedSmartSleeveController, InjectionEvent
from sensor_interfaces import SensorType, FluidMeasurement
from config_system import ConfigurationManager


class MultiSensorSmartSleeveDemo:
    """Demonstration of multi-sensor smart sleeve capabilities"""
    
    def __init__(self):
        self.controller = None
        self.config_manager = ConfigurationManager("multi_sensor_config.json")
        
    def setup_pressure_based_system(self) -> bool:
        """Setup smart sleeve with pressure sensor at syringe tip"""
        print("🔧 Setting up PRESSURE-BASED Smart Sleeve System")
        print("=" * 60)
        
        # Create communication interface (WiFi for this example)
        comm_interface = CommunicationFactory.create_interface(CommunicationProtocol.WIFI)
        
        # Create enhanced controller
        self.controller = EnhancedSmartSleeveController("sleeve_pressure_001", comm_interface)
        
        # Configure multiple sensors
        sensor_config = {
            "tip_pressure": {
                "type": "PRESSURE",
                "parameters": {
                    "sensitivity": 1.0,
                    "pressure_range_kpa": (0, 50),
                    "filter_enabled": True
                }
            },
            "volume_capacitive": {
                "type": "CAPACITIVE", 
                "parameters": {
                    "volume_factor": 0.15,
                    "temperature_compensation": True
                }
            },
            "flow_monitor": {
                "type": "FLOW_RATE",
                "parameters": {
                    "calibration_factor": 1.1
                }
            },
            "optical_detector": {
                "type": "OPTICAL",
                "parameters": {
                    "wavelength_nm": 850
                }
            }
        }
        
        # Communication configuration
        comm_config = {
            'protocol': 'wifi',
            'port': 8080,
            'use_tcp': True
        }
        
        # Initialize system
        success = self.controller.initialize_system(
            comm_config, 
            "192.168.1.100",  # Hub address
            sensor_config
        )
        
        if success:
            print("✓ Multi-sensor system initialized successfully")
            print(f"✓ Sensors configured: {list(sensor_config.keys())}")
            
            # Configure syringe parameters for a 10mL syringe
            self.controller.configure_syringe({
                'initial_volume': 10.0,  # mL
                'diameter_mm': 14.5,     # Standard 10mL syringe
                'capacitive_factor': 0.12,
                'pressure_volume_factor': 0.08
            })
            
            # Configure detection thresholds
            self.controller.configure_injection_thresholds({
                'pressure_kpa': 1.5,      # Sensitive pressure detection
                'flow_rate_ml_per_s': 0.05,  # Detect very low flow rates
                'capacitance_change': 0.8,
                'optical_change': 0.08
            })
            
            # Register event callbacks
            self.controller.register_callback("monitoring_started", self._on_monitoring_started)
            self.controller.register_callback("injection_detected", self._on_injection_detected)
            
        return success
    
    def _on_monitoring_started(self, initial_measurement: FluidMeasurement):
        """Callback when monitoring starts"""
        print(f"\n📊 Monitoring Started")
        print(f"   Initial Volume: {initial_measurement.initial_volume_ml:.2f} mL")
        print(f"   Sensor Fusion Confidence: {initial_measurement.confidence_level:.1%}")
        print(f"   Active Sensors: {len(initial_measurement.sensor_readings)}")
        
        for reading in initial_measurement.sensor_readings:
            print(f"   {reading.sensor_type.upper()}: {reading.primary_value:.2f} {reading.unit}")
    
    def _on_injection_detected(self, injection_event: InjectionEvent, measurement: FluidMeasurement):
        """Callback when injection is detected"""
        print(f"\n🎯 INJECTION DETECTED - Multi-Sensor Fusion")
        print(f"   Detection Time: {time.strftime('%H:%M:%S', time.localtime(injection_event.timestamp))}")
        print(f"   Detection Method: {injection_event.detection_method}")
        print(f"   Triggering Sensors: {', '.join(injection_event.triggering_sensors)}")
        print(f"   Confidence Level: {injection_event.confidence_level:.1%}")
        print(f"   Pressure at Detection: {injection_event.pressure_at_detection_kpa:.2f} kPa")
        print(f"   Flow Rate: {injection_event.flow_rate_ml_per_sec:.3f} mL/s")
        print(f"   Volume Dispensed: {measurement.dispensed_volume_ml:.3f} mL")
        print(f"   Remaining Volume: {measurement.current_volume_ml:.2f} mL")
        print(f"   🔄 Triggering quantification algorithm on Connected OR HUB")
    
    async def run_pressure_demo(self, duration_seconds: int = 45):
        """Run pressure-based detection demo"""
        if not self.controller:
            print("❌ Controller not initialized")
            return
        
        print(f"\n🔍 Starting pressure-based monitoring for {duration_seconds} seconds...")
        print("   This demo simulates ICG injection with tip pressure sensing")
        
        # Start monitoring with 5mL syringe
        if not self.controller.start_monitoring(5.0):
            print("❌ Failed to start monitoring")
            return
        
        start_time = time.time()
        last_status_time = 0
        
        try:
            while (time.time() - start_time) < duration_seconds:
                # Monitor injection
                measurement = self.controller.monitor_injection()
                
                if measurement:
                    current_time = time.time()
                    
                    # Print status every 10 seconds
                    if current_time - last_status_time >= 10:
                        print(f"\n⏱️  Status Update - {int(current_time - start_time)}s elapsed")
                        print(f"   Current Volume: {measurement.current_volume_ml:.2f} mL")
                        print(f"   Dispensed: {measurement.dispensed_volume_ml:.3f} mL")
                        print(f"   Pressure: {measurement.pressure_kpa:.2f} kPa")
                        print(f"   Flow Rate: {measurement.flow_rate_ml_per_sec:.3f} mL/s")
                        print(f"   Fusion Confidence: {measurement.confidence_level:.1%}")
                        last_status_time = current_time
                
                # Check system status
                status = self.controller.get_sensor_status()
                if not status['is_connected']:
                    print("⚠️  Warning: Lost connection to hub")
                
                await asyncio.sleep(0.1)  # 100ms monitoring interval
                
        except KeyboardInterrupt:
            print("\n⏹️  Demo stopped by user")
        
        finally:
            self.controller.shutdown()
    
    def setup_optical_flow_system(self) -> bool:
        """Setup smart sleeve with optical + flow rate sensors"""
        print("\n🔧 Setting up OPTICAL + FLOW RATE System")
        print("=" * 60)
        
        # Create communication interface (UWB for this example)
        comm_interface = CommunicationFactory.create_interface(CommunicationProtocol.UWB)
        
        # Create enhanced controller
        self.controller = EnhancedSmartSleeveController("sleeve_optical_001", comm_interface)
        
        # Configure sensors focused on optical and flow detection
        sensor_config = {
            "flow_sensor": {
                "type": "FLOW_RATE",
                "parameters": {
                    "calibration_factor": 1.05
                }
            },
            "optical_transmittance": {
                "type": "OPTICAL",
                "parameters": {
                    "wavelength_nm": 940  # Near-IR good for ICG detection
                }
            },
            "backup_ultrasonic": {
                "type": "ULTRASONIC",
                "parameters": {
                    "frequency_khz": 40
                }
            }
        }
        
        # UWB communication configuration
        comm_config = {
            'protocol': 'uwb',
            'ranging_enabled': True
        }
        
        success = self.controller.initialize_system(
            comm_config, 
            "uwb_anchor_01",
            sensor_config
        )
        
        if success:
            print("✓ Optical + Flow Rate system initialized")
            print("✓ UWB communication with ranging enabled")
        
        return success
    
    async def run_optical_flow_demo(self, duration_seconds: int = 30):
        """Run optical + flow rate detection demo"""
        if not self.controller:
            return
        
        print(f"\n🔍 Optical + Flow Rate monitoring for {duration_seconds} seconds...")
        
        if not self.controller.start_monitoring(8.0):  # 8mL syringe
            return
        
        start_time = time.time()
        
        try:
            while (time.time() - start_time) < duration_seconds:
                measurement = self.controller.monitor_injection()
                
                if measurement:
                    # Show optical and flow-specific data
                    if int(time.time()) % 8 == 0:  # Every 8 seconds
                        print(f"📡 Optical transmission: {measurement.sensor_readings[1].primary_value:.3f}")
                        print(f"🌊 Flow rate: {measurement.flow_rate_ml_per_sec:.3f} mL/s")
                
                await asyncio.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\n⏹️  Optical demo stopped")
        
        finally:
            self.controller.shutdown()
    
    def demonstrate_sensor_comparison(self):
        """Demonstrate the advantages of different sensor approaches"""
        print("\n📋 SENSOR APPROACH COMPARISON")
        print("=" * 60)
        
        comparison_data = {
            "Pressure Sensor (Tip)": {
                "pros": [
                    "Direct measurement of injection pressure",
                    "Immediate detection of injection start",
                    "Works with any fluid type",
                    "High temporal resolution",
                    "Minimal interference from external factors"
                ],
                "cons": [
                    "Requires modification to syringe/needle interface",
                    "May affect sterile field",
                    "Pressure calibration needed for volume estimation"
                ],
                "best_for": "Real-time injection detection and flow control"
            },
            
            "Capacitive Sensor": {
                "pros": [
                    "Non-invasive measurement",
                    "Direct volume estimation",
                    "Good for conductive fluids",
                    "Simple integration"
                ],
                "cons": [
                    "Affected by fluid conductivity",
                    "Temperature sensitive",
                    "May not work with all syringe materials"
                ],
                "best_for": "Volume monitoring of conductive fluids"
            },
            
            "Optical Sensor": {
                "pros": [
                    "Non-contact measurement",
                    "Good for ICG detection (fluorescent)",
                    "High precision",
                    "Can detect different fluid types"
                ],
                "cons": [
                    "Affected by ambient light",
                    "Requires optical access",
                    "May need wavelength tuning"
                ],
                "best_for": "Presence detection and fluid type identification"
            },
            
            "Flow Rate Sensor": {
                "pros": [
                    "Direct flow measurement",
                    "Real-time injection rate",
                    "Independent of fluid properties",
                    "High accuracy"
                ],
                "cons": [
                    "Requires inline installation",
                    "May create pressure drop",
                    "Complex mechanical integration"
                ],
                "best_for": "Precise flow control and rate monitoring"
            },
            
            "Multi-Sensor Fusion": {
                "pros": [
                    "Highest accuracy and reliability",
                    "Redundancy for safety",
                    "Comprehensive measurement",
                    "Adaptable to different scenarios"
                ],
                "cons": [
                    "Higher complexity and cost",
                    "More calibration required",
                    "Increased power consumption"
                ],
                "best_for": "Critical medical applications requiring maximum reliability"
            }
        }
        
        for sensor_type, info in comparison_data.items():
            print(f"\n{sensor_type}:")
            print(f"  ✅ Pros: {', '.join(info['pros'][:2])}")  # Show first 2 pros
            print(f"  ❌ Cons: {', '.join(info['cons'][:2])}")  # Show first 2 cons
            print(f"  🎯 Best for: {info['best_for']}")
    
    def show_system_status(self):
        """Display comprehensive system status"""
        if not self.controller:
            print("❌ No active controller")
            return
        
        status = self.controller.get_sensor_status()
        
        print(f"\n📊 SYSTEM STATUS - {status['device_id']}")
        print("=" * 50)
        print(f"Connected: {'✅' if status['is_connected'] else '❌'}")
        print(f"Monitoring: {'✅' if status['is_monitoring'] else '❌'}")
        print(f"Signal Strength: {status['signal_strength']:.1f}%")
        print(f"Sensor Count: {status['sensor_count']}")
        print(f"Fusion Enabled: {'✅' if status['fusion_enabled'] else '❌'}")
        
        print(f"\n📡 SENSOR STATUS:")
        for sensor_id, sensor_info in status['sensors'].items():
            calibrated = "✅" if sensor_info['calibrated'] else "❌"
            self_test = "✅" if sensor_info['self_test_passed'] else "❌"
            print(f"  {sensor_id} ({sensor_info['type']}): Calibrated {calibrated}, Self-test {self_test}")


async def main():
    """Main demonstration function"""
    demo = MultiSensorSmartSleeveDemo()
    
    print("🩺 Enhanced Smart Sleeve System - Multi-Sensor Demonstration")
    print("=" * 70)
    
    # Show sensor comparison
    demo.demonstrate_sensor_comparison()
    
    # Demo 1: Pressure-based system (primary recommendation)
    print(f"\n{'='*70}")
    print("DEMO 1: PRESSURE SENSOR AT SYRINGE TIP")
    print("This is the recommended approach for direct injection detection")
    print("="*70)
    
    if demo.setup_pressure_based_system():
        demo.show_system_status()
        await demo.run_pressure_demo(30)  # 30 second demo
    
    # Brief pause between demos
    await asyncio.sleep(2)
    
    # Demo 2: Optical + Flow system
    print(f"\n{'='*70}")
    print("DEMO 2: OPTICAL + FLOW RATE SENSORS")
    print("Alternative approach using optical detection and flow monitoring")
    print("="*70)
    
    if demo.setup_optical_flow_system():
        await demo.run_optical_flow_demo(20)  # 20 second demo
    
    print("\n✅ Multi-sensor demonstration completed!")
    
    # Summary
    print(f"\n📝 SUMMARY:")
    print("1. Pressure sensor at tip provides most direct injection detection")
    print("2. Multi-sensor fusion improves accuracy and reliability") 
    print("3. System supports any communication protocol (WiFi, BLE, UWB, etc.)")
    print("4. Modular design allows easy sensor reconfiguration")
    print("5. Real-time sensor fusion provides confidence metrics")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Multi-sensor demo interrupted by user")
    except Exception as e:
        print(f"\n❌ Demo failed: {e}")
        import traceback
        traceback.print_exc()