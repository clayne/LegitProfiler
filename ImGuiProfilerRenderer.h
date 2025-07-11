#pragma once

#include "ProfilerTask.h"
#include "imgui.h"
#include <array>
#include <map>
#include <vector>

namespace ImGuiUtils
{
  inline glm::vec2 Vec2(ImVec2 vec)
  {
    return glm::vec2(vec.x, vec.y);
  }
  class ProfilerGraph
  {
  public:
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText;

    ProfilerGraph(size_t framesCount)
    {
      frames.resize(framesCount);
      for (auto &frame : frames)
        frame.tasks.reserve(100);
      frameWidth = 3;
      frameSpacing = 1;
      useColoredLegendText = false;
    }

    void LoadFrameData(const legit::ProfilerTask *tasks, size_t count)
    {
      auto &currFrame = frames[currFrameIndex];
      currFrame.tasks.resize(0);
      for (size_t taskIndex = 0; taskIndex < count; taskIndex++)
      {
        if (taskIndex == 0)
          currFrame.tasks.push_back(tasks[taskIndex]);
        else
        {
          if (tasks[taskIndex - 1].color != tasks[taskIndex].color || tasks[taskIndex - 1].name != tasks[taskIndex].name)
          {
            currFrame.tasks.push_back(tasks[taskIndex]);
          }
          else
          {
            currFrame.tasks.back().endTime = tasks[taskIndex].endTime;
          }
        }
      }
      currFrame.taskStatsIndex.resize(currFrame.tasks.size());

      for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
      {
        auto &task = currFrame.tasks[taskIndex];
        auto it = taskNameToStatsIndex.find(task.name);
        if (it == taskNameToStatsIndex.end())
        {
          taskNameToStatsIndex[task.name] = taskStats.size();
          TaskStats taskStat;
          taskStat.maxTime = -1.0;
          taskStats.push_back(taskStat);
        }
        currFrame.taskStatsIndex[taskIndex] = taskNameToStatsIndex[task.name];
      }
      currFrameIndex = (currFrameIndex + 1) % frames.size();

      RebuildTaskStats(currFrameIndex, 300/*frames.size()*/);
    }

    void RenderTimings(int graphWidth, int legendWidth, int height, int frameIndexOffset, float maxFrameTime)
    {
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      const glm::vec2 widgetPos = Vec2(ImGui::GetCursorScreenPos());
      RenderGraph(drawList, widgetPos, glm::vec2(graphWidth, height), frameIndexOffset, maxFrameTime);
      RenderLegend(drawList, widgetPos + glm::vec2(graphWidth, 0.0f), glm::vec2(legendWidth, height), frameIndexOffset, maxFrameTime);
      ImGui::Dummy(ImVec2(float(graphWidth + legendWidth), float(height)));
    }

    /*void bla()
    {

    }*/
  private:
    void RebuildTaskStats(size_t endFrame, size_t framesCount)
    {
      for (auto &taskStat : taskStats)
      {
        taskStat.maxTime = -1.0f;
        taskStat.priorityOrder = size_t(-1);
        taskStat.onScreenIndex = size_t(-1);
      }

      for (size_t frameNumber = 0; frameNumber < framesCount; frameNumber++)
      {
        size_t frameIndex = (endFrame - 1 - frameNumber + frames.size()) % frames.size();
        auto &frame = frames[frameIndex];
        for (size_t taskIndex = 0; taskIndex < frame.tasks.size(); taskIndex++)
        {
          auto &task = frame.tasks[taskIndex];
          auto &stats = taskStats[frame.taskStatsIndex[taskIndex]];
          stats.maxTime = std::max(stats.maxTime, task.endTime - task.startTime);
        }
      }
      std::vector<size_t> statPriorities;
      statPriorities.resize(taskStats.size());
      for(size_t statIndex = 0; statIndex < taskStats.size(); statIndex++)
        statPriorities[statIndex] = statIndex;

      std::sort(statPriorities.begin(), statPriorities.end(), [this](size_t left, size_t right) {return taskStats[left].maxTime > taskStats[right].maxTime; });
      for (size_t statNumber = 0; statNumber < taskStats.size(); statNumber++)
      {
        size_t statIndex = statPriorities[statNumber];
        taskStats[statIndex].priorityOrder = statNumber;
      }
    }
    void RenderGraph(ImDrawList *drawList, glm::vec2 graphPos, glm::vec2 graphSize, size_t frameIndexOffset, float maxFrameTime)
    {
      Rect(drawList, graphPos, graphPos + graphSize, 0xffffffff, false);
      float heightThreshold = 1.0f;

      for (size_t frameNumber = 0; frameNumber < frames.size(); frameNumber++)
      {
        size_t frameIndex = (currFrameIndex - frameIndexOffset - 1 - frameNumber + 2 * frames.size()) % frames.size();

        glm::vec2 framePos = graphPos + glm::vec2(graphSize.x - 1 - frameWidth - (frameWidth + frameSpacing) * frameNumber, graphSize.y - 1);
        if (framePos.x < graphPos.x + 1)
          break;
        glm::vec2 taskPos = framePos + glm::vec2(0.0f, 0.0f);
        auto &frame = frames[frameIndex];
        for (const auto& task : frame.tasks)
        {
          float taskStartHeight = (float(task.startTime) / maxFrameTime) * graphSize.y;
          float taskEndHeight = (float(task.endTime) / maxFrameTime) * graphSize.y;
          //taskMaxCosts[task.name] = std::max(taskMaxCosts[task.name], task.endTime - task.startTime);
          if (abs(taskEndHeight - taskStartHeight) > heightThreshold)
            Rect(drawList, taskPos + glm::vec2(0.0f, -taskStartHeight), taskPos + glm::vec2(frameWidth, -taskEndHeight), task.color, true);
        }
      }
    }
    void RenderLegend(ImDrawList *drawList, glm::vec2 legendPos, glm::vec2 legendSize, size_t frameIndexOffset, float maxFrameTime)
    {
      float markerLeftRectMargin = 3.0f;
      float markerLeftRectWidth = 5.0f;
      float markerMidWidth = 30.0f;
      float markerRightRectWidth = 10.0f;
      float markerRigthRectMargin = 3.0f;
      float markerRightRectHeight = 10.0f;
      float markerRightRectSpacing = 4.0f;
      float nameOffset = 30.0f;
      glm::vec2 textMargin = glm::vec2(5.0f, -3.0f);

      auto &currFrame = frames[(currFrameIndex - frameIndexOffset - 1 + 2 * frames.size()) % frames.size()];
      size_t maxTasksCount = size_t(legendSize.y / (markerRightRectHeight + markerRightRectSpacing));

      for (auto &taskStat : taskStats)
      {
        taskStat.onScreenIndex = size_t(-1);
      }

      size_t tasksToShow = std::min<size_t>(taskStats.size(), maxTasksCount);
      size_t tasksShownCount = 0;
      for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
      {
        auto &task = currFrame.tasks[taskIndex];
        auto &stat = taskStats[currFrame.taskStatsIndex[taskIndex]];

        if (stat.priorityOrder >= tasksToShow)
          continue;

        if (stat.onScreenIndex == size_t(-1))
        {
          stat.onScreenIndex = tasksShownCount++;
        }
        else
          continue;
        float taskStartHeight = (float(task.startTime) / maxFrameTime) * legendSize.y;
        float taskEndHeight = (float(task.endTime) / maxFrameTime) * legendSize.y;

        glm::vec2 markerLeftRectMin = legendPos + glm::vec2(markerLeftRectMargin, legendSize.y);
        glm::vec2 markerLeftRectMax = markerLeftRectMin + glm::vec2(markerLeftRectWidth, 0.0f);
        markerLeftRectMin.y -= taskStartHeight;
        markerLeftRectMax.y -= taskEndHeight;

        glm::vec2 markerRightRectMin = legendPos + glm::vec2(markerLeftRectMargin + markerLeftRectWidth + markerMidWidth, legendSize.y - markerRigthRectMargin - (markerRightRectHeight + markerRightRectSpacing) * stat.onScreenIndex);
        glm::vec2 markerRightRectMax = markerRightRectMin + glm::vec2(markerRightRectWidth, -markerRightRectHeight);
        RenderTaskMarker(drawList, markerLeftRectMin, markerLeftRectMax, markerRightRectMin, markerRightRectMax, task.color);

        uint32_t textColor = useColoredLegendText ? task.color : legit::Colors::imguiText;// task.color;

        float taskTimeMs = float(task.endTime - task.startTime);
        std::ostringstream timeText;
        timeText.precision(2);
        timeText << std::fixed << std::string("[") << (taskTimeMs * 1000.0f);

        Text(drawList, markerRightRectMax + textMargin, textColor, timeText.str().c_str());
        Text(drawList, markerRightRectMax + textMargin + glm::vec2(nameOffset, 0.0f), textColor, (std::string("ms] ") + task.name).c_str());
      }

      /*
      struct PriorityEntry
      {
        bool isUsed;
        legit::ProfilerTask task;
      };
      std::map<std::string, PriorityEntry> priorityEntries;
      for (auto priorityTask : priorityTasks)
      {
        PriorityEntry entry;
        entry.task = frames[priorityTask.frameIndex].tasks[priorityTask.taskIndex];
        entry.isUsed = false;
        priorityEntries[entry.task.name] = entry;
      }
      size_t shownTasksCount = 0;
      for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
      {
        auto &task = currFrame.tasks[taskIndex];
        auto it = priorityEntries.find(task.name);
        if (it != priorityEntries.end() && !it->second.isUsed)
        {
          it->second.isUsed = true;

          float taskStartHeight = (float(task.startTime) / maxFrameTime) * legendSize.y;
          float taskEndHeight = (float(task.endTime) / maxFrameTime) * legendSize.y;


          glm::vec2 markerLeftRectMin = legendPos + glm::vec2(markerLeftRectMargin, legendSize.y);
          glm::vec2 markerLeftRectMax = markerLeftRectMin + glm::vec2(markerLeftRectWidth, 0.0f);
          markerLeftRectMin.y -= taskStartHeight;
          markerLeftRectMax.y -= taskEndHeight;

          glm::vec2 markerRightRectMin = legendPos + glm::vec2(markerLeftRectMargin + markerLeftRectWidth + markerMidWidth, legendSize.y - markerRigthRectMargin - (markerRightRectHeight + markerRightRectSpacing) * shownTasksCount);
          glm::vec2 markerRightRectMax = markerRightRectMin + glm::vec2(markerRightRectWidth, -markerRightRectHeight);
          RenderTaskMarker(drawList, markerLeftRectMin, markerLeftRectMax, markerRightRectMin, markerRightRectMax, task.color);

          uint32_t textColor = legit::Colors::imguiText;// task.color;

          float taskTimeMs = float(task.endTime - task.startTime);
          std::ostringstream timeText;
          timeText.precision(2);
          timeText << std::fixed << std::string("[") << (taskTimeMs * 1000.0f);

          Text(drawList, markerRightRectMax + textMargin, textColor, timeText.str().c_str());
          Text(drawList, markerRightRectMax + textMargin + glm::vec2(nameOffset, 0.0f), textColor, (std::string("ms] ") + task.name).c_str());
          shownTasksCount++;
        }
      }*/

      /*for (size_t priorityTaskIndex = 0; priorityTaskIndex < priorityTasks.size(); priorityTaskIndex++)
      {
        auto &priorityTask = priorityTasks[priorityTaskIndex];
        auto &globalTask = frames[priorityTask.frameIndex].tasks[priorityTask.taskIndex];

        size_t lastFrameTaskIndex = currFrame.FindTask(globalTask.name);

        glm::vec2 taskPos = legendPos + marginSpacing + glm::vec2(0.0f, markerHeight) + glm::vec2(0.0f, (markerHeight + itemSpacing) * priorityTaskIndex);
        Rect(drawList, taskPos, taskPos + glm::vec2(markerHeight, -markerHeight), task.color, true);
        Text(drawList, taskPos + textOffset, 0xffffffff, task.name.c_str());
      }*/
    }

    static void Rect(ImDrawList *drawList, glm::vec2 minPoint, glm::vec2 maxPoint, uint32_t col, bool filled = true)
    {
      if(filled)
        drawList->AddRectFilled(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
      else
        drawList->AddRect(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
    }
    static void Text(ImDrawList *drawList, glm::vec2 point, uint32_t col, const char *text)
    {
      drawList->AddText(ImVec2(point.x, point.y), col, text);
    }
    static void Triangle(ImDrawList *drawList, std::array<glm::vec2, 3> points, uint32_t col, bool filled = true)
    {
      if (filled)
        drawList->AddTriangleFilled(ImVec2(points[0].x, points[0].y), ImVec2(points[1].x, points[1].y), ImVec2(points[2].x, points[2].y), col);
      else
        drawList->AddTriangle(ImVec2(points[0].x, points[0].y), ImVec2(points[1].x, points[1].y), ImVec2(points[2].x, points[2].y), col);
    }
    static void RenderTaskMarker(ImDrawList *drawList, glm::vec2 leftMinPoint, glm::vec2 leftMaxPoint, glm::vec2 rightMinPoint, glm::vec2 rightMaxPoint, uint32_t col)
    {
      Rect(drawList, leftMinPoint, leftMaxPoint, col, true);
      Rect(drawList, rightMinPoint, rightMaxPoint, col, true);
      std::array<ImVec2, 4> points = {
        ImVec2(leftMaxPoint.x, leftMinPoint.y),
        ImVec2(leftMaxPoint.x, leftMaxPoint.y),
        ImVec2(rightMinPoint.x, rightMaxPoint.y),
        ImVec2(rightMinPoint.x, rightMinPoint.y)
      };
      drawList->AddConvexPolyFilled(points.data(), int(points.size()), col);
    }
    struct FrameData
    {
      /*void BuildPriorityTasks(size_t maxPriorityTasksCount)
      {
        priorityTaskIndices.clear();
        std::set<std::string> usedTaskNames;

        for (size_t priorityIndex = 0; priorityIndex < maxPriorityTasksCount; priorityIndex++)
        {
          size_t bestTaskIndex = size_t(-1);
          for (size_t taskIndex = 0; taskIndex < tasks.size(); taskIndex++)
          {
            auto &task = tasks[taskIndex];
            auto it = usedTaskNames.find(tasks[taskIndex].name);
            if (it == usedTaskNames.end() && (bestTaskIndex == size_t(-1) || tasks[bestTaskIndex].GetLength() < task.GetLength()))
            {
              bestTaskIndex = taskIndex;
            }
          }
          if (bestTaskIndex == size_t(-1))
            break;
          priorityTaskIndices.push_back(bestTaskIndex);
          usedTaskNames.insert(tasks[bestTaskIndex].name);
        }
      }*/
      std::vector<legit::ProfilerTask> tasks;
      std::vector<size_t> taskStatsIndex;
      //std::vector<size_t> priorityTaskIndices;
    };

    struct TaskStats
    {
      double maxTime;
      size_t priorityOrder;
      size_t onScreenIndex;
    };
    std::vector<TaskStats> taskStats;
    std::map<std::string, size_t> taskNameToStatsIndex;

    /*struct PriorityTask
    {
      size_t frameIndex;
      size_t taskIndex;
    };
    std::vector<PriorityTask> priorityTasks;*/
    std::vector<FrameData> frames;
    size_t currFrameIndex = 0;
  };

  class ProfilersWindow
  {
  public:
    ProfilersWindow(float maxFrameTime = 1.0f / 60.0f):
      cpuGraph(300),
      gpuGraph(300),
      maxFrameTime(maxFrameTime)
    {
      stopProfiling = false;
      frameOffset = 0;
      frameWidth = 3;
      frameSpacing = 1;
      useColoredLegendText = true;
      prevFpsFrameTime = std::chrono::system_clock::now();
      fpsFramesCount = 0;
      avgFrameTime = 1.0f;
    }
    void Render()
    {
      fpsFramesCount++;
      auto currFrameTime = std::chrono::system_clock::now();
      {
        float fpsDeltaTime = std::chrono::duration<float>(currFrameTime - prevFpsFrameTime).count();
        if (fpsDeltaTime > 0.5f)
        {
          this->avgFrameTime = fpsDeltaTime / float(fpsFramesCount);
          fpsFramesCount = 0;
          prevFpsFrameTime = currFrameTime;
        }
      }

      std::stringstream title;
      title.precision(2);
      title << std::fixed << "Legit profiler [" << 1.0f / avgFrameTime << "fps\t" << avgFrameTime * 1000.0f << "ms]###ProfilerWindow";
      //###AnimatedTitle
      ImGui::Begin(title.str().c_str(), 0, ImGuiWindowFlags_NoScrollbar);
      ImVec2 canvasSize = ImGui::GetContentRegionAvail();

      int sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
      int maxGraphHeight = 300;
      int availableGraphHeight = (int(canvasSize.y) - sizeMargin) / 2;
      int graphHeight = std::min(maxGraphHeight, availableGraphHeight);
      int legendWidth = 200;
      int graphWidth = int(canvasSize.x) - legendWidth;
      gpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset, maxFrameTime);
      cpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset, maxFrameTime);
      if (graphHeight * 2 + sizeMargin + sizeMargin < canvasSize.y)
      {
        ImGui::Columns(2);
        size_t textSize = 50;
        ImGui::Checkbox("Stop profiling", &stopProfiling);
        //ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - textSize);
        ImGui::Checkbox("Colored legend text", &useColoredLegendText);
        ImGui::DragInt("Frame offset", &frameOffset, 1.0f, 0, 400);
        ImGui::NextColumn();

        ImGui::SliderInt("Frame width", &frameWidth, 1, 4);
        ImGui::SliderInt("Frame spacing", &frameSpacing, 0, 2);
        ImGui::SliderFloat("Transparency", &ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w, 0.0f, 1.0f);
        ImGui::Columns(1);
      }
      if (!stopProfiling)
        frameOffset = 0;
      gpuGraph.frameWidth = frameWidth;
      gpuGraph.frameSpacing = frameSpacing;
      gpuGraph.useColoredLegendText = useColoredLegendText;
      cpuGraph.frameWidth = frameWidth;
      cpuGraph.frameSpacing = frameSpacing;
      cpuGraph.useColoredLegendText = useColoredLegendText;

      ImGui::End();
    }
    bool stopProfiling;
    int frameOffset;
    ProfilerGraph cpuGraph;
    ProfilerGraph gpuGraph;
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText;
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    TimePoint prevFpsFrameTime;
    size_t fpsFramesCount;
    float avgFrameTime;
    float maxFrameTime;
  };
}

