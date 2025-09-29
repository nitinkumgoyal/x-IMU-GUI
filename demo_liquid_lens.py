#!/usr/bin/env python3
"""
Liquid Lens Camera Demo
Complete demonstration of out-of-the-box liquid lens usage
"""

import sys
import time
import argparse
from liquid_lens_camera import LiquidLensCamera, CameraConfig
from liquid_lens_hardware import LiquidLensDriver, LiquidLensController

def demo_basic_usage():
    """Basic liquid lens camera demonstration"""
    print("=== Basic Liquid Lens Demo ===")
    
    # Create camera with default settings
    camera = LiquidLensCamera()
    
    try:
        if not camera.initialize():
            print("Failed to initialize camera")
            return False
        
        print("Camera initialized successfully!")
        print("Starting 5-second preview...")
        
        # Demonstrate different focus modes
        modes = [
            ("Macro Mode", camera.macro_mode),
            ("Telephoto Mode", camera.telephoto_mode),
            ("Mid Focus", lambda: camera.set_focus_position(2048))
        ]
        
        for mode_name, mode_func in modes:
            print(f"\nSwitching to {mode_name}...")
            mode_func()
            
            # Capture a few frames to show the effect
            for i in range(5):
                frame = camera.capture_frame()
                if frame is not None:
                    print(f"  Frame {i+1} captured")
                time.sleep(0.2)
        
        print("\nBasic demo completed!")
        return True
        
    except Exception as e:
        print(f"Demo error: {e}")
        return False
    finally:
        camera.cleanup()

def demo_focus_stacking():
    """Demonstrate focus stacking capability"""
    print("\n=== Focus Stacking Demo ===")
    
    camera = LiquidLensCamera()
    
    try:
        if not camera.initialize():
            return False
        
        print("Capturing focus stack sequence...")
        photos = camera.focus_stack_sequence(num_steps=5)
        
        print(f"Captured {len(photos)} photos for focus stacking:")
        for photo in photos:
            print(f"  - {photo}")
            
        return True
        
    except Exception as e:
        print(f"Focus stacking error: {e}")
        return False
    finally:
        camera.cleanup()

def demo_hardware_control():
    """Demonstrate low-level hardware control"""
    print("\n=== Hardware Control Demo ===")
    
    driver = LiquidLensDriver()
    controller = LiquidLensController(driver)
    
    try:
        if not controller.connect():
            print("Hardware control demo running in simulation mode")
        
        print("Testing hardware calibration...")
        controller.calibrate()
        
        print("Testing focus distance control...")
        distances = [50, 100, 200, 500, 1000]  # mm
        
        for distance in distances:
            print(f"Setting focus to {distance}mm")
            controller.set_focus_distance(distance)
            time.sleep(0.3)
        
        print("Testing percentage-based control...")
        percentages = [0, 25, 50, 75, 100]
        
        for pct in percentages:
            print(f"Setting focus to {pct}%")
            controller.set_focus_percentage(pct)
            time.sleep(0.3)
        
        print("Performing focus sweep...")
        positions = controller.focus_sweep(steps=8, delay_ms=200)
        print(f"Focus sweep completed with {len(positions)} positions")
        
        return True
        
    except Exception as e:
        print(f"Hardware demo error: {e}")
        return False
    finally:
        controller.disconnect()

def demo_interactive_mode():
    """Interactive demonstration mode"""
    print("\n=== Interactive Liquid Lens Demo ===")
    print("This will open a live camera preview with keyboard controls")
    print("Make sure you have a camera connected!")
    
    config = CameraConfig(width=1280, height=720, fps=30)
    camera = LiquidLensCamera(config=config)
    
    try:
        if not camera.initialize():
            return False
        
        print("\nStarting interactive preview...")
        camera.start_preview()
        return True
        
    except Exception as e:
        print(f"Interactive demo error: {e}")
        return False
    finally:
        camera.cleanup()

def main():
    """Main demo launcher"""
    parser = argparse.ArgumentParser(description="Liquid Lens Camera Demo")
    parser.add_argument("--mode", choices=["basic", "stacking", "hardware", "interactive", "all"],
                       default="all", help="Demo mode to run")
    parser.add_argument("--camera-id", type=int, default=0, help="Camera device ID")
    
    args = parser.parse_args()
    
    print("Liquid Lens Camera - Out of the Box Demo")
    print("========================================")
    
    demos = {
        "basic": demo_basic_usage,
        "stacking": demo_focus_stacking, 
        "hardware": demo_hardware_control,
        "interactive": demo_interactive_mode
    }
    
    success = True
    
    if args.mode == "all":
        # Run all demos except interactive
        for demo_name, demo_func in demos.items():
            if demo_name != "interactive":
                if not demo_func():
                    success = False
                time.sleep(1)
        
        # Ask if user wants interactive demo
        try:
            response = input("\nRun interactive camera demo? (y/n): ").lower()
            if response.startswith('y'):
                demo_interactive_mode()
        except KeyboardInterrupt:
            pass
            
    else:
        success = demos[args.mode]()
    
    print(f"\nDemo completed {'successfully' if success else 'with errors'}")
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())