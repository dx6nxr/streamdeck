"use client";

import { useState } from "react";

interface AppBoxProps {
  appName: string;
  onDragStart: (appName: string) => void;
}

export function AppBox({ appName, onDragStart }: AppBoxProps) {
  const [isDragging, setIsDragging] = useState(false);
  
  const handleDragStart = (e: React.DragEvent) => {
    setIsDragging(true);
    // Set data transfer
    e.dataTransfer.setData('application/json', JSON.stringify({ appName }));
    // Call the parent handler
    onDragStart(appName);
  };
  
  const handleDragEnd = () => {
    setIsDragging(false);
  };
  
  return (
    <div 
      className={`border rounded p-2 text-center cursor-grab hover:bg-accent transition-colors ${isDragging ? 'opacity-50 bg-accent' : ''}`}
      draggable="true"
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
    >
      {appName}
    </div>
  );
} 