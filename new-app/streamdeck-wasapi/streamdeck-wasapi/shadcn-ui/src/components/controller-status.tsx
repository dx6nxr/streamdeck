"use client";

import { useState } from "react";
import { Button } from "@/components/ui/button";

interface Slider {
  id: number;
  value: number;
  label: string;
}

interface Button {
  id: number;
  pressed: boolean;
  label: string;
}

interface ControllerStatusProps {
  connected: boolean;
  sliders: Slider[];
  buttons: Button[];
}

export function ControllerStatus({ connected, sliders, buttons }: ControllerStatusProps) {
  const [isCollapsed, setIsCollapsed] = useState(false);
  
  const toggleCollapse = () => {
    setIsCollapsed(!isCollapsed);
  };
  
  return (
    <div className="border-t p-4">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-xl font-semibold">Controller Status</h2>
        <Button variant="ghost" size="sm" onClick={toggleCollapse}>
          {isCollapsed ? '▲' : '▼'}
        </Button>
      </div>
      
      {!isCollapsed && (
        <>
          <div className="mb-4">
            <div className={`inline-block px-3 py-1 rounded-full text-sm ${connected ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>
              {connected ? 'Connected' : 'Not connected'}
            </div>
          </div>
          
          <div className="grid grid-cols-2 gap-8">
            <div>
              <h3 className="text-lg font-medium mb-2">Sliders</h3>
              <div className="space-y-4">
                {sliders.map((slider) => (
                  <div key={slider.id} className="space-y-2">
                    <div className="flex justify-between">
                      <span>{slider.label}</span>
                      <span>{slider.value}%</span>
                    </div>
                    <div className="h-2 bg-secondary rounded-full">
                      <div 
                        className="h-2 bg-primary rounded-full" 
                        style={{ width: `${slider.value}%` }}
                      ></div>
                    </div>
                  </div>
                ))}
                {sliders.length === 0 && (
                  <div className="text-sm text-muted-foreground">No sliders connected</div>
                )}
              </div>
            </div>
            
            <div>
              <h3 className="text-lg font-medium mb-2">Buttons</h3>
              <div className="flex gap-4">
                {buttons.map((button) => (
                  <div key={button.id} className="text-center">
                    <div className={`w-5 h-5 rounded-full mx-auto mb-1 ${button.pressed ? 'bg-primary' : 'bg-secondary border-2 border-muted-foreground'}`}></div>
                    <div className="text-xs">{button.label}</div>
                  </div>
                ))}
                {buttons.length === 0 && (
                  <div className="text-sm text-muted-foreground">No buttons connected</div>
                )}
              </div>
            </div>
          </div>
        </>
      )}
    </div>
  );
} 