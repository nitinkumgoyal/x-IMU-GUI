#!/usr/bin/env python3
"""
Liquid Lens Camera Implementation
Out-of-the-box usage example for liquid lens camera control
"""

import time
import cv2
import numpy as np
from dataclasses import dataclass
from typing import Optional, Tuple
import threading

@dataclass
class CameraConfig:
    """Camera configuration for liquid lens"""
    width: int = 1920
    height: int = 1080
    fps: int = 30
    focus_range: Tuple[int, int] = (0, 4095)
    auto_focus_enabled: bool = True

class LiquidLensCamera:
    """
    Liquid Lens Camera Controller
    Provides out-of-the-box functionality for liquid lens cameras
    """
    
    def __init__(self, camera_id: int = 0, config: Optional[CameraConfig] = None):
        self.camera_id = camera_id
        self.config = config or CameraConfig()
        self.cap = None
        self.current_focus = 2048  # Mid-range focus
        self.is_recording = False
        self.focus_lock = threading.Lock()
        
    def initialize(self) -> bool:
        """Initialize camera and liquid lens"""
        try:
            self.cap = cv2.VideoCapture(self.camera_id)
            if not self.cap.isOpened():
                print(f"Error: Could not open camera {self.camera_id}")
                return False
                
            # Set camera properties
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.config.width)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.config.height)
            self.cap.set(cv2.CAP_PROP_FPS, self.config.fps)
            
            # Initialize liquid lens (simulated)
            self._init_liquid_lens()
            
            print(f"Camera initialized: {self.config.width}x{self.config.height} @ {self.config.fps}fps")
            return True
            
        except Exception as e:
            print(f"Initialization error: {e}")
            return False
    
    def _init_liquid_lens(self):
        """Initialize liquid lens hardware (simulated)"""
        # In real implementation, this would initialize I2C communication
        # with the liquid lens driver chip
        print("Liquid lens initialized")
        self.set_focus_position(self.current_focus)
    
    def set_focus_position(self, position: int) -> bool:
        """
        Set focus position
        Args:
            position: Focus value (0=macro, 4095=telephoto)
        """
        with self.focus_lock:
            min_pos, max_pos = self.config.focus_range
            position = max(min_pos, min(max_pos, position))
            
            # Simulate liquid lens control
            # In real implementation: send I2C commands to lens driver
            self.current_focus = position
            
            focus_percentage = (position / max_pos) * 100
            print(f"Focus set to: {position} ({focus_percentage:.1f}%)")
            return True
    
    def macro_mode(self):
        """Switch to macro photography mode"""
        print("Switching to macro mode...")
        return self.set_focus_position(0)
    
    def telephoto_mode(self):
        """Switch to telephoto mode"""
        print("Switching to telephoto mode...")
        return self.set_focus_position(4095)
    
    def auto_focus(self, frame: np.ndarray) -> bool:
        """
        Perform auto-focus using contrast detection
        Args:
            frame: Current camera frame
        """
        if frame is None:
            return False
            
        # Convert to grayscale for focus analysis
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Calculate focus measure using Laplacian variance
        focus_measure = cv2.Laplacian(gray, cv2.CV_64F).var()
        
        # Simple auto-focus algorithm
        best_focus = self.current_focus
        best_measure = focus_measure
        
        # Search for optimal focus (simplified)
        search_range = 200
        step = 50
        
        for offset in range(-search_range, search_range + 1, step):
            test_position = self.current_focus + offset
            if 0 <= test_position <= 4095:
                self.set_focus_position(test_position)
                time.sleep(0.01)  # Allow lens to settle
                
                # In real implementation, capture new frame here
                test_measure = focus_measure  # Placeholder
                
                if test_measure > best_measure:
                    best_measure = test_measure
                    best_focus = test_position
        
        # Set to best focus position found
        self.set_focus_position(best_focus)
        return True
    
    def capture_frame(self) -> Optional[np.ndarray]:
        """Capture a single frame"""
        if not self.cap:
            return None
            
        ret, frame = self.cap.read()
        return frame if ret else None
    
    def start_preview(self):
        """Start live preview with focus control"""
        if not self.cap:
            print("Camera not initialized")
            return
            
        print("Starting preview... Press 'q' to quit")
        print("Controls:")
        print("  'm' - Macro mode")
        print("  't' - Telephoto mode") 
        print("  'a' - Auto focus")
        print("  '+'/'-' - Manual focus adjust")
        print("  'r' - Start/stop recording")
        
        fourcc = cv2.VideoWriter_fourcc(*'XVID')
        out = None
        
        while True:
            frame = self.capture_frame()
            if frame is None:
                break
                
            # Display current focus info
            focus_text = f"Focus: {self.current_focus} ({(self.current_focus/4095)*100:.1f}%)"
            cv2.putText(frame, focus_text, (10, 30), 
                       cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            
            if self.is_recording:
                cv2.putText(frame, "REC", (frame.shape[1]-100, 30),
                           cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
                if out:
                    out.write(frame)
            
            cv2.imshow('Liquid Lens Camera', frame)
            
            # Handle keyboard input
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('m'):
                self.macro_mode()
            elif key == ord('t'):
                self.telephoto_mode()
            elif key == ord('a'):
                print("Auto-focusing...")
                self.auto_focus(frame)
            elif key == ord('+') or key == ord('='):
                self.set_focus_position(self.current_focus + 100)
            elif key == ord('-'):
                self.set_focus_position(self.current_focus - 100)
            elif key == ord('r'):
                if not self.is_recording:
                    # Start recording
                    timestamp = int(time.time())
                    filename = f"liquid_lens_recording_{timestamp}.avi"
                    out = cv2.VideoWriter(filename, fourcc, self.config.fps, 
                                        (self.config.width, self.config.height))
                    self.is_recording = True
                    print(f"Started recording: {filename}")
                else:
                    # Stop recording
                    if out:
                        out.release()
                        out = None
                    self.is_recording = False
                    print("Stopped recording")
        
        # Cleanup
        if out:
            out.release()
        cv2.destroyAllWindows()
    
    def capture_photo_sequence(self, focus_positions: list, filename_prefix: str = "liquid_lens"):
        """
        Capture photos at different focus positions
        Useful for focus stacking or depth analysis
        """
        photos = []
        
        for i, position in enumerate(focus_positions):
            self.set_focus_position(position)
            time.sleep(0.1)  # Allow lens to settle
            
            frame = self.capture_frame()
            if frame is not None:
                filename = f"{filename_prefix}_focus_{position}_{i:03d}.jpg"
                cv2.imwrite(filename, frame)
                photos.append(filename)
                print(f"Captured: {filename}")
        
        return photos
    
    def focus_stack_sequence(self, num_steps: int = 10) -> list:
        """
        Capture sequence for focus stacking
        Args:
            num_steps: Number of focus steps to capture
        """
        focus_positions = np.linspace(0, 4095, num_steps, dtype=int)
        return self.capture_photo_sequence(focus_positions, "focus_stack")
    
    def cleanup(self):
        """Clean up resources"""
        if self.cap:
            self.cap.release()
        cv2.destroyAllWindows()

def main():
    """Demo usage of liquid lens camera"""
    # Create camera instance
    config = CameraConfig(width=1280, height=720, fps=30)
    camera = LiquidLensCamera(camera_id=0, config=config)
    
    try:
        # Initialize camera
        if not camera.initialize():
            return
        
        # Start live preview
        camera.start_preview()
        
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        camera.cleanup()

if __name__ == "__main__":
    main()