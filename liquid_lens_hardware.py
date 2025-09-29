#!/usr/bin/env python3
"""
Liquid Lens Hardware Interface
Low-level hardware control for liquid lens modules
"""

import time
import struct
from typing import Optional, Tuple
try:
    import smbus2
    I2C_AVAILABLE = True
except ImportError:
    I2C_AVAILABLE = False
    print("Warning: smbus2 not available. I2C functionality disabled.")

class LiquidLensDriver:
    """
    Hardware driver for liquid lens control
    Supports common liquid lens modules via I2C
    """
    
    # Common I2C addresses for liquid lens controllers
    DEFAULT_I2C_ADDRESS = 0x48
    BACKUP_I2C_ADDRESS = 0x49
    
    # Register addresses (example for typical liquid lens driver)
    REG_FOCUS_POSITION = 0x00
    REG_POWER_MODE = 0x01
    REG_STATUS = 0x02
    REG_CALIBRATION = 0x03
    
    def __init__(self, i2c_bus: int = 1, i2c_address: int = None):
        self.i2c_bus = i2c_bus
        self.i2c_address = i2c_address or self.DEFAULT_I2C_ADDRESS
        self.bus = None
        self.is_connected = False
        
    def connect(self) -> bool:
        """Establish I2C connection to liquid lens"""
        if not I2C_AVAILABLE:
            print("I2C not available - using simulation mode")
            self.is_connected = True
            return True
            
        try:
            self.bus = smbus2.SMBus(self.i2c_bus)
            
            # Test connection by reading status register
            status = self.bus.read_byte_data(self.i2c_address, self.REG_STATUS)
            self.is_connected = True
            print(f"Liquid lens connected at 0x{self.i2c_address:02X}, status: 0x{status:02X}")
            return True
            
        except Exception as e:
            print(f"Failed to connect to liquid lens: {e}")
            # Try backup address
            try:
                self.i2c_address = self.BACKUP_I2C_ADDRESS
                status = self.bus.read_byte_data(self.i2c_address, self.REG_STATUS)
                self.is_connected = True
                print(f"Liquid lens connected at backup address 0x{self.i2c_address:02X}")
                return True
            except:
                self.is_connected = False
                return False
    
    def disconnect(self):
        """Close I2C connection"""
        if self.bus:
            self.bus.close()
        self.is_connected = False
    
    def set_focus_raw(self, value: int) -> bool:
        """
        Set raw focus value (0-4095 for 12-bit resolution)
        Args:
            value: Raw focus position value
        """
        if not self.is_connected:
            print(f"Simulated focus set to: {value}")
            return True
            
        try:
            # Split 12-bit value into two bytes
            high_byte = (value >> 8) & 0x0F
            low_byte = value & 0xFF
            
            # Write to focus position register
            self.bus.write_i2c_block_data(self.i2c_address, self.REG_FOCUS_POSITION, 
                                        [high_byte, low_byte])
            
            # Wait for lens to settle
            time.sleep(0.001)  # 1ms settling time
            return True
            
        except Exception as e:
            print(f"Error setting focus: {e}")
            return False
    
    def get_focus_position(self) -> Optional[int]:
        """Read current focus position"""
        if not self.is_connected:
            return 2048  # Simulated mid-position
            
        try:
            data = self.bus.read_i2c_block_data(self.i2c_address, self.REG_FOCUS_POSITION, 2)
            position = ((data[0] & 0x0F) << 8) | data[1]
            return position
            
        except Exception as e:
            print(f"Error reading focus position: {e}")
            return None
    
    def calibrate(self) -> bool:
        """Perform lens calibration sequence"""
        if not self.is_connected:
            print("Simulated calibration completed")
            return True
            
        try:
            # Start calibration sequence
            self.bus.write_byte_data(self.i2c_address, self.REG_CALIBRATION, 0x01)
            
            # Wait for calibration to complete (typically 100-500ms)
            timeout = 50  # 500ms timeout
            for _ in range(timeout):
                status = self.bus.read_byte_data(self.i2c_address, self.REG_STATUS)
                if status & 0x01 == 0:  # Calibration complete bit
                    print("Liquid lens calibration completed")
                    return True
                time.sleep(0.01)
            
            print("Calibration timeout")
            return False
            
        except Exception as e:
            print(f"Calibration error: {e}")
            return False

class LiquidLensController:
    """
    High-level controller for liquid lens with optical calculations
    """
    
    def __init__(self, driver: LiquidLensDriver):
        self.driver = driver
        self.focal_length_base = 4.0  # Base focal length in mm
        self.focus_range_mm = (10, float('inf'))  # Focus range in mm
        
    def connect(self) -> bool:
        """Connect to liquid lens hardware"""
        return self.driver.connect()
    
    def disconnect(self):
        """Disconnect from hardware"""
        self.driver.disconnect()
    
    def set_focus_distance(self, distance_mm: float) -> bool:
        """
        Set focus to specific distance in millimeters
        Args:
            distance_mm: Focus distance (10mm to infinity)
        """
        if distance_mm < 10:
            distance_mm = 10  # Minimum focus distance
        
        # Convert distance to raw focus value using lens equation
        # 1/f = 1/u + 1/v (where f=focal length, u=object distance, v=image distance)
        if distance_mm == float('inf'):
            focus_value = 4095  # Maximum value for infinity
        else:
            # Simplified conversion (in practice, this would use calibration data)
            normalized = 1.0 - (1.0 / (distance_mm / 10.0))  # Normalize to 0-1
            focus_value = int(normalized * 4095)
        
        return self.driver.set_focus_raw(focus_value)
    
    def set_focus_percentage(self, percentage: float) -> bool:
        """
        Set focus as percentage (0% = macro, 100% = infinity)
        Args:
            percentage: Focus percentage (0.0 to 100.0)
        """
        percentage = max(0.0, min(100.0, percentage))
        focus_value = int((percentage / 100.0) * 4095)
        return self.driver.set_focus_raw(focus_value)
    
    def macro_mode(self) -> bool:
        """Set to macro focus (closest focus)"""
        return self.driver.set_focus_raw(0)
    
    def infinity_mode(self) -> bool:
        """Set to infinity focus"""
        return self.driver.set_focus_raw(4095)
    
    def calibrate(self) -> bool:
        """Calibrate the liquid lens"""
        return self.driver.calibrate()
    
    def focus_sweep(self, start_pct: float = 0, end_pct: float = 100, 
                   steps: int = 20, delay_ms: int = 50) -> list:
        """
        Perform focus sweep for testing or focus stacking
        Args:
            start_pct: Starting focus percentage
            end_pct: Ending focus percentage  
            steps: Number of steps
            delay_ms: Delay between steps in milliseconds
        Returns:
            List of focus positions used
        """
        positions = []
        step_size = (end_pct - start_pct) / (steps - 1)
        
        for i in range(steps):
            percentage = start_pct + (i * step_size)
            if self.set_focus_percentage(percentage):
                focus_value = int((percentage / 100.0) * 4095)
                positions.append(focus_value)
                time.sleep(delay_ms / 1000.0)
            
        return positions

def main():
    """Test liquid lens hardware interface"""
    # Create driver and controller
    driver = LiquidLensDriver(i2c_bus=1, i2c_address=0x48)
    controller = LiquidLensController(driver)
    
    try:
        # Connect to hardware
        if not controller.connect():
            print("Failed to connect to liquid lens")
            return
        
        # Calibrate lens
        print("Calibrating lens...")
        controller.calibrate()
        
        # Test different focus modes
        print("Testing macro mode...")
        controller.macro_mode()
        time.sleep(1)
        
        print("Testing infinity mode...")
        controller.infinity_mode()
        time.sleep(1)
        
        print("Testing focus distances...")
        distances = [50, 100, 500, 1000]  # mm
        for dist in distances:
            print(f"Setting focus to {dist}mm")
            controller.set_focus_distance(dist)
            time.sleep(0.5)
        
        print("Performing focus sweep...")
        positions = controller.focus_sweep(steps=10, delay_ms=100)
        print(f"Focus sweep completed: {len(positions)} positions")
        
    except KeyboardInterrupt:
        print("\nTest interrupted")
    finally:
        controller.disconnect()

if __name__ == "__main__":
    main()