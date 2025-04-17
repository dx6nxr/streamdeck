"use client";

import { useState, useRef } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";

interface GroupBoxProps {
  groupName: string;
  groupIndex: number;
  apps: string[];
  onDrop: (groupIndex: number, appName: string) => void;
  onRename: (groupIndex: number, newName: string) => void;
}

export function GroupBox({ 
  groupName, 
  groupIndex, 
  apps, 
  onDrop, 
  onRename 
}: GroupBoxProps) {
  const [isRenaming, setIsRenaming] = useState(false);
  const [isDragOver, setIsDragOver] = useState(false);
  const [newName, setNewName] = useState(groupName);
  const inputRef = useRef<HTMLInputElement>(null);
  
  // Drag and drop handling
  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(true);
  };
  
  const handleDragLeave = () => {
    setIsDragOver(false);
  };
  
  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(false);
    
    try {
      const data = JSON.parse(e.dataTransfer.getData('application/json'));
      if (data && data.appName) {
        onDrop(groupIndex, data.appName);
      }
    } catch (error) {
      console.error("Error parsing dropped data:", error);
    }
  };
  
  // Rename handling
  const handleTitleClick = () => {
    setIsRenaming(true);
    // Focus the input in the next tick after rendering
    setTimeout(() => {
      if (inputRef.current) {
        inputRef.current.focus();
        inputRef.current.select();
      }
    }, 0);
  };
  
  const handleNameChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setNewName(e.target.value);
  };
  
  const handleBlur = () => {
    finishRenaming();
  };
  
  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      finishRenaming();
    } else if (e.key === 'Escape') {
      // Cancel rename
      setNewName(groupName);
      setIsRenaming(false);
    }
  };
  
  const finishRenaming = () => {
    if (newName.trim() !== '' && newName !== groupName) {
      onRename(groupIndex, newName);
    }
    setIsRenaming(false);
  };
  
  return (
    <Card 
      className={`flex-1 min-w-[150px] h-full ${isDragOver ? 'bg-accent/20 ring-2 ring-primary/30' : ''}`}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
    >
      <CardHeader className="p-3">
        {isRenaming ? (
          <Input
            ref={inputRef}
            value={newName}
            onChange={handleNameChange}
            onBlur={handleBlur}
            onKeyDown={handleKeyDown}
            className="text-center text-sm py-1 h-auto"
          />
        ) : (
          <CardTitle className="text-sm text-center cursor-pointer" onDoubleClick={handleTitleClick}>
            {groupName}
          </CardTitle>
        )}
      </CardHeader>
      <CardContent className="flex flex-col gap-2 p-3">
        {apps.length > 0 ? (
          apps.map((app, index) => (
            <div key={index} className="border rounded p-2 text-center bg-background text-sm">
              {app}
            </div>
          ))
        ) : (
          <div className="text-center text-muted-foreground text-sm py-4">
            Drop apps here
          </div>
        )}
      </CardContent>
    </Card>
  );
} 